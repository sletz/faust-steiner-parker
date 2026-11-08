// Reference renderer for faust-steiner-parker.
//
// Drives the original model, ref/SPModel.h (copied unchanged from
// sofiavallejob/steiner-parker-filter, plugin/Source/SPModel.h), and writes
// raw little-endian doubles, so that the Faust port can be compared with it
// sample by sample.
//
//   sp_ref core   <dest> <fs> <cut> <cv> <res> <t1> <in.f64> <out.f64>
//       the circuit alone at the rate fs: one tick per input sample, the input
//       taken as the source voltage after IC1A (volts), the output Vo1 (volts).
//       res is the RES pot position (0..1), not the knob.
//   sp_ref plugin <dest> <fs> <os> <cut> <cv> <res> <t1> <drive_db> <out_db> <in.f64> <out.f64>
//       the plugin chain of the JSFX version (jsfx/Steiner-Parker Filter.jsfx):
//       5 ms smoothing of cut/cv/res, the controls updated once per host
//       sample, 1.0 = 5 V at the jack, IC1A inverting, oversampling by os
//       (1, 2 or 4) with the JSFX's two biquads on each side, output stage,
//       /5, output gain. res is the knob (0..1), scaled by 0.78 as in the plugin.
//   sp_ref time   <dest> <fs> <os> <seconds>
//       the plugin chain in stereo on a 55 Hz saw, timed: prints the cost.
//
// Iteration statistics of the per-sample Newton go to stderr.
//
// SPDX-License-Identifier: AGPL-3.0-only (the model it includes is AGPL-3.0)

#include "SPModel.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static std::vector<double> readF64(const char* path) {
    std::vector<double> v;
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::perror(path); std::exit(1); }
    double x;
    while (std::fread(&x, sizeof x, 1, f) == 1) v.push_back(x);
    std::fclose(f);
    return v;
}

static void writeF64(const char* path, const std::vector<double>& v) {
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror(path); std::exit(1); }
    std::fwrite(v.data(), sizeof(double), v.size(), f);
    std::fclose(f);
}

struct Stats {
    long n = 0, sum = 0; int mx = 0; long hist[51] = {};
    void add(int it) { ++n; sum += it; mx = std::max(mx, it); ++hist[std::min(it, 50)]; }
    void print() const {
        std::fprintf(stderr, "newton: %ld ticks, mean %.3f iterations, max %d; histogram", n, n ? double(sum) / n : 0.0, mx);
        for (int i = 0; i <= 50; ++i) if (hist[i]) std::fprintf(stderr, " %d:%ld", i, hist[i]);
        std::fprintf(stderr, "\n");
    }
};

// The JSFX's anti-aliasing filters: RBJ low-pass biquads, transposed direct form II.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    void lowpass(double fc, double fsr, double q) {
        double w0 = 2 * M_PI * fc / fsr, al = std::sin(w0) / (2 * q), cw = std::cos(w0), a0 = 1 + al;
        b0 = (1 - cw) / 2 / a0; b1 = (1 - cw) / a0; b2 = (1 - cw) / 2 / a0;
        a1 = -2 * cw / a0; a2 = (1 - al) / a0;
    }
};
struct BiquadState {
    double s0 = 0, s1 = 0;
    double tick(const Biquad& c, double x) {
        double y = c.b0 * x + s0;
        s0 = c.b1 * x - c.a1 * y + s1;
        s1 = c.b2 * x - c.a2 * y;
        return y;
    }
};

struct Plugin {
    static constexpr double kResPotMax = 0.78;
    sp::Circuit circuit;
    sp::Controls controls;
    Biquad cf1, cf2;
    BiquadState st[2][4];
    int os = 1;
    double smooth = 0, cutS = 0, cvS = 0, resS = 0;
    double cutT = 0, cvT = 0, resT = 0, t1 = 0, drive = 1, outG = 1;
    Stats stats;

    void setup(int dest, double fs, int osf, double cut, double cv, double res, double t1_,
               double driveDb, double outDb) {
        os = osf;
        cutS = cutT = cut; cvS = cvT = cv; resS = resT = res; t1 = t1_;
        drive = std::pow(10.0, driveDb / 20.0); outG = std::pow(10.0, outDb / 20.0);
        smooth = 1.0 - std::exp(-1.0 / (0.005 * fs));
        controls.update(cutS, cvS, resS * kResPotMax, t1);
        circuit.setup(dest, fs * os, controls, 2);
        cf1.lowpass(0.45 * fs, fs * os, 0.5412);
        cf2.lowpass(0.45 * fs, fs * os, 1.3066);
    }
    double aa(double x, BiquadState* s) { return s[1].tick(cf2, s[0].tick(cf1, x)); }
    void frameControls() {
        cutS += smooth * (cutT - cutS); cvS += smooth * (cvT - cvS); resS += smooth * (resT - resS);
        controls.update(cutS, cvS, resS * kResPotMax, t1);
    }
    double processCh(int ch, double x) {
        double vs = -5.0 * x * drive;
        double y;
        if (os == 1) {
            y = circuit.tick(ch, vs, controls); stats.add(circuit.lastIterations);
        } else {
            double acc = 0;
            for (int k = 0; k < os; ++k) {
                y = circuit.tick(ch, aa(k == 0 ? vs * os : 0.0, st[ch]), controls);
                stats.add(circuit.lastIterations);
                acc = aa(y, st[ch] + 2);
            }
            y = acc;
        }
        return sp::Circuit::outputStage(y) / 5.0 * outG;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: see the header of sp_ref.cpp\n"); return 1; }
    if (!std::strcmp(argv[1], "core") && argc == 10) {
        int dest = std::atoi(argv[2]); double fs = std::atof(argv[3]);
        sp::Controls c; c.update(std::atof(argv[4]), std::atof(argv[5]), std::atof(argv[6]), std::atof(argv[7]));
        sp::Circuit ckt; ckt.setup(dest, fs, c, 1);
        auto in = readF64(argv[8]);
        std::vector<double> out(in.size());
        Stats s;
        for (size_t i = 0; i < in.size(); ++i) { out[i] = ckt.tick(0, in[i], c); s.add(ckt.lastIterations); }
        s.print();
        writeF64(argv[9], out);
        return 0;
    }
    if (!std::strcmp(argv[1], "plugin") && argc == 13) {
        Plugin p;
        p.setup(std::atoi(argv[2]), std::atof(argv[3]), std::atoi(argv[4]), std::atof(argv[5]), std::atof(argv[6]),
                std::atof(argv[7]), std::atof(argv[8]), std::atof(argv[9]), std::atof(argv[10]));
        auto in = readF64(argv[11]);
        std::vector<double> out(in.size());
        for (size_t i = 0; i < in.size(); ++i) { p.frameControls(); out[i] = p.processCh(0, in[i]); }
        p.stats.print();
        writeF64(argv[12], out);
        return 0;
    }
    if (!std::strcmp(argv[1], "time") && argc == 6) {
        double fs = std::atof(argv[3]); int os = std::atoi(argv[4]); double sec = std::atof(argv[5]);
        Plugin p;
        p.setup(std::atoi(argv[2]), fs, os, 0.51, 0.0, 0.4, 0.45, 0.0, -6.0);
        long n = long(sec * fs);
        double sink = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < n; ++i) {
            double ph = std::fmod(i * 55.0 / fs, 1.0), x = 0.9 * (2 * ph - 1);
            p.frameControls();
            sink += p.processCh(0, x) + p.processCh(1, -x);
        }
        double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("C++ original: %.3f s of stereo audio at %g Hz x%d in %.4f s: %.1fx real time, %.1f ns per stereo sample\n",
                    sec, fs, os, dt, sec / dt, dt / n * 1e9);
        if (sink == 12345.0) std::printf(" ");  // keeps the output used
        p.stats.print();
        return 0;
    }
    std::fprintf(stderr, "usage: see the header of sp_ref.cpp\n");
    return 1;
}
