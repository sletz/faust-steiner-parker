// Architecture file for the CPU benchmark and the rendering check of the
// Faust-generated C++ (faust -double -a tools/bench_arch.cpp).
//
//   sp_faust time   <fs> <seconds>                 stereo 55 Hz saw (0.9, -0.9), timed
//   sp_faust render <fs> <in.f64> <out.f64> [path=value ...]
//
// The timed signal and settings are those of `sp_ref time`.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "faust/dsp/dsp.h"
#include "faust/gui/MapUI.h"
#include "faust/gui/meta.h"

<<includeIntrinsic>>
<<includeclass>>

static std::vector<double> readF64(const char* p) {
    std::vector<double> v; FILE* f = std::fopen(p, "rb"); if (!f) { std::perror(p); std::exit(1); }
    double x; while (std::fread(&x, sizeof x, 1, f) == 1) v.push_back(x); std::fclose(f); return v;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: see tools/bench_arch.cpp\n"); return 1; }
    int fs = std::atoi(argv[2]);
    mydsp d; MapUI ui; d.buildUserInterface(&ui); d.init(fs);
    const int bs = 256;
    std::vector<FAUSTFLOAT> in0(bs), in1(bs), out0(bs), out1(bs);
    FAUSTFLOAT* ins[2] = { in0.data(), in1.data() }; FAUSTFLOAT* outs[2] = { out0.data(), out1.data() };
    if (!std::strcmp(argv[1], "time")) {
        double sec = std::atof(argv[3]); long n = long(sec * fs), done = 0; double sink = 0, dt = 0;
        while (done < n) {
            int k = int(std::min<long>(bs, n - done));
            for (int i = 0; i < k; ++i) { double ph = std::fmod((done + i) * 55.0 / fs, 1.0), x = 0.9 * (2 * ph - 1); in0[i] = x; in1[i] = -x; }
            auto t0 = std::chrono::steady_clock::now();
            d.compute(k, ins, outs);
            dt += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            for (int i = 0; i < k; ++i) sink += out0[i] + out1[i];
            done += k;
        }
        if (sink == 12345.0) std::printf(" ");  // keeps the output used
        std::printf("Faust (C++ backend): %.3f s of stereo audio at %d Hz in %.4f s: %.1fx real time, %.1f ns per stereo sample\n",
                    sec, fs, dt, sec / dt, dt / n * 1e9);
        return 0;
    }
    if (!std::strcmp(argv[1], "render") && argc >= 5) {
        for (int a = 5; a < argc; ++a) { std::string s(argv[a]); auto e = s.find('='); ui.setParamValue(s.substr(0, e), std::atof(s.c_str() + e + 1)); }
        auto x = readF64(argv[3]); std::vector<double> y; FILE* f = std::fopen(argv[4], "wb");
        for (size_t p = 0; p < x.size(); p += bs) {
            int k = int(std::min<size_t>(bs, x.size() - p));
            for (int i = 0; i < k; ++i) { in0[i] = x[p + i]; in1[i] = x[p + i]; }
            d.compute(k, ins, outs);
            for (int i = 0; i < k; ++i) { double v = out0[i]; std::fwrite(&v, sizeof v, 1, f); }
        }
        std::fclose(f); return 0;
    }
    return 1;
}
