// Steiner-Parker Filter
// Copyright (C) 2026 Sofia Vallejo
// SPDX-License-Identifier: AGPL-3.0-only
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License version 3. See LICENSE.

// Steiner-Parker VCF, nonlinear wave digital model.
// Circuit: Vult Eurorack build (L. Laguna Ruiz 2017, after Yusynth).
// Same netlist and solver as the Python reference (sp/circuit.py, sp/solver.py):
// capacitors are WDF leaves (Rc = T/2C, b[n] = a[n-1]); the rest is one
// nonlinear root reduced to 5 ports (4 merged diode groups + the resonance
// feedback) and solved with Newton each sample.
#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace sp {

constexpr int NN = 11, NP = 5, NC = 6, NZ = 4 + NC;
enum Node { L = 0, R, A, M, B, W, NB, NCn, CT, P, S };

constexpr double VT = 0.02585, IS_D = 2.52e-9, N_D = 1.752;
constexpr double VCC = 12.0, VEE = -12.0, VSAT = 10.5, VBE = 0.65, GMIN = 1e-12;
constexpr double R15 = 1000.0, GAIN1 = 1.0 + 47000.0 / 22000.0, GOUT = 220000.0 / 10000.0;

inline double railClip(double x, double& dydx) {
    double u = std::abs(x) / VSAT, u2 = u * u, u4 = u2 * u2, u8 = u4 * u4;
    double k = std::pow(1.0 + u8, -0.125);
    dydx = k / (1.0 + u8);
    return x * k;
}
inline double railClip(double x) { double d; return railClip(x, d); }

struct Controls {
    double i1 = 0, i2 = 0, kfb = 0;
    // cut 0..1 (CUT knob), cv in volts, res 0..1 (RES knob), t1 0..1 (V/oct trim)
    void update(double cut, double cv, double res, double t1) {
        double vcut = VCC * 50000.0 / (47000.0 + 50000.0) * cut;
        double rb = 220.0 + t1 * 2000.0;
        double g = 1.0 / 150000.0 + 3.0 / 220000.0 + 1.0 / rb;
        double vb = (vcut / 150000.0 + cv / 220000.0) / g;
        double it = (0.0 - VBE - VEE) / (3900.0 + 82.0);
        double x = std::clamp(vb / VT, -60.0, 60.0);
        i1 = it / (1.0 + std::exp(-x));
        i2 = it - i1;
        double lo = std::max(res, 1e-6) * 100000.0, up = (1.0 - res) * 100000.0;
        double upe = up > 0 ? up * 100000.0 / (up + 100000.0) : 0.0;
        double z = upe + lo;
        double vtop = z / (3900.0 + z);
        kfb = vtop * lo / z * 1e6 / (220000.0 + 1e6);
    }
};

class Circuit {
public:
    // dest: 0 = input A (low-pass), 1 = B (band-pass), 2 = C (high-pass)
    void setup(int dest, double fsInternal, const Controls& c, int numChannels) {
        build(dest);
        // --- DC operating point (capacitors open)
        Mat11 Gi = invert(G);
        reduce(Gi);
        std::array<double, NN> r = rhs;
        r[L] -= c.i1; r[R] -= c.i2;
        std::array<double, NN> v0 = mul(Gi, r);
        std::array<double, NP> u0 = measure(v0);
        std::array<double, NN> v{};
        for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
            auto& s = st[ch];
            s.u = { 0.5, 0.5, 0.5, 0.5, 0.0 };
            newton(s.u, u0, c, 200, 1e-9);
            std::array<double, NP> cur, der;
            currents(s.u, c, cur, der);
            for (int i = 0; i < NN; ++i) {
                double acc = v0[i];
                for (int k = 0; k < NP; ++k) acc -= Wm[i][k] * cur[k];
                v[i] = acc;
            }
            for (int k = 0; k < NC; ++k) s.b[k] = port(v, k);   // i = 0 -> a = b = v
        }
        // --- runtime: add capacitor port conductances
        Mat11 Gc = G;
        for (int k = 0; k < NC; ++k) {
            Rc[k] = 1.0 / (2.0 * fsInternal * C[k]);
            stamp(Gc, cp[k], cq[k], 1.0 / Rc[k]);
        }
        Gi = invert(Gc);
        reduce(Gi);
        precompute(Gi);
    }

    // One sample at the internal (oversampled) rate. vs = source voltage after
    // IC1A (inverting). Returns Vo1, the IC1C output.
    double tick(int ch, double vs, const Controls& c) {
        auto& s = st[ch];
        std::array<double, NZ> z{};
        z[0] = 1.0; z[1] = vs; z[2] = c.i1; z[3] = c.i2;
        for (int k = 0; k < NC; ++k) z[4 + k] = s.b[k];
        std::array<double, NP> u0{};
        for (int j = 0; j < NP; ++j) {
            double acc = 0;
            for (int k = 0; k < NZ; ++k) acc += Au[j][k] * z[k];
            u0[j] = acc;
        }
        lastIterations = newton(s.u, u0, c, 50, 1e-9);
        std::array<double, NP> cur, der;
        currents(s.u, c, cur, der);
        for (int k = 0; k < NC; ++k) {
            double vp = 0;
            for (int m = 0; m < NZ; ++m) vp += Ac[k][m] * z[m];
            for (int m = 0; m < NP; ++m) vp -= Wc[k][m] * cur[m];
            s.b[k] = 2.0 * vp - s.b[k];
        }
        return railClip(GAIN1 * s.u[4]);
    }

    static double outputStage(double vo1) { return railClip(-GOUT * vo1); }
    bool isFinite(int ch) const {
        for (double x : st[ch].u) if (!std::isfinite(x)) return false;
        for (double x : st[ch].b) if (!std::isfinite(x)) return false;
        return true;
    }
    int lastIterations = 0;

private:
    using Mat11 = std::array<std::array<double, NN>, NN>;
    struct State { std::array<double, NP> u{}; std::array<double, NC> b{}; };
    std::array<State, 2> st;

    Mat11 G{};
    std::array<double, NN> rhs{}, src{};
    std::array<int, NC> cp{}, cq{};
    std::array<double, NC> C{}, Rc{};
    int ncap = 0;
    const int dA[4] = { R, B, M, A }, dK[4] = { B, M, A, L };
    const double dMult[4] = { 3, 2, 2, 3 };
    double K[NP][NP]{}, Wm[NN][NP]{}, Au[NP][NZ]{}, Ac[NC][NZ]{}, Wc[NC][NP]{};

    static void stamp(Mat11& m, int a, int b, double g) {
        if (a >= 0) m[a][a] += g;
        if (b >= 0) m[b][b] += g;
        if (a >= 0 && b >= 0) { m[a][b] -= g; m[b][a] -= g; }
    }
    void g2(int a, int b, double r) { stamp(G, a, b, 1.0 / r); }
    void gsrc(int a, double r, std::array<double, NN>& vec, double val) { G[a][a] += 1.0 / r; vec[a] += val / r; }
    void addcap(int p, int q, double c) { cp[ncap] = p; cq[ncap] = q; C[ncap] = c; ++ncap; }

    void build(int dest) {
        G = {}; rhs = {}; src = {}; ncap = 0;
        for (int i = 0; i < NN; ++i) G[i][i] = GMIN;
        gsrc(CT, 390.0, rhs, VCC);            // R10
        g2(CT, W, 1000.0);                     // R11
        g2(W, L, 1800.0 + 500.0);              // R9 + half of T2 (centred)
        g2(W, R, 1800.0 + 500.0);              // R13 + half of T2
        g2(NCn, -1, 1000.0);                   // R12
        g2(NB, -1, R15);                       // R15 (Vfb side is nonlinear)
        g2(P, -1, 2.2e6);                      // R14
        addcap(CT, -1, 47e-6);                 // C7
        addcap(A, NB, 1.5e-9);                 // C9
        addcap(B, NB, 1.5e-9);                 // C11
        addcap(M, NCn, 1.5e-9);                // C10
        addcap(M, P, 680e-9);                  // C12
        if (dest == 0)      { gsrc(S, 47000.0, src, 1.0); g2(S, -1, 180000.0); addcap(S, W, 2.2e-6); }   // R6, R7, C5
        else if (dest == 1) { gsrc(S, 47000.0, src, 1.0); g2(S, -1, 180000.0); addcap(S, NB, 2.2e-6); }  // R6, R8, C6
        else                { gsrc(NCn, 47000.0, src, 1.0); g2(S, -1, 1e6); addcap(S, -1, 1e-9); }       // R6 into Nc; S unused
    }

    static Mat11 invert(const Mat11& a) {
        double aug[NN][2 * NN];
        for (int i = 0; i < NN; ++i)
            for (int j = 0; j < NN; ++j) { aug[i][j] = a[i][j]; aug[i][NN + j] = (i == j); }
        for (int k = 0; k < NN; ++k) {
            int p = k;
            for (int i = k + 1; i < NN; ++i) if (std::abs(aug[i][k]) > std::abs(aug[p][k])) p = i;
            if (p != k) for (int j = 0; j < 2 * NN; ++j) std::swap(aug[k][j], aug[p][j]);
            double piv = aug[k][k];
            for (int j = 0; j < 2 * NN; ++j) aug[k][j] /= piv;
            for (int i = 0; i < NN; ++i) {
                if (i == k) continue;
                double f = aug[i][k];
                if (f != 0) for (int j = 0; j < 2 * NN; ++j) aug[i][j] -= f * aug[k][j];
            }
        }
        Mat11 out{};
        for (int i = 0; i < NN; ++i) for (int j = 0; j < NN; ++j) out[i][j] = aug[i][NN + j];
        return out;
    }
    static std::array<double, NN> mul(const Mat11& m, const std::array<double, NN>& x) {
        std::array<double, NN> y{};
        for (int i = 0; i < NN; ++i) { double s = 0; for (int j = 0; j < NN; ++j) s += m[i][j] * x[j]; y[i] = s; }
        return y;
    }
    std::array<double, NP> measure(const std::array<double, NN>& v) const {
        std::array<double, NP> u{};
        for (int k = 0; k < 4; ++k) u[k] = v[dA[k]] - v[dK[k]];
        u[4] = v[P];
        return u;
    }
    double port(const std::array<double, NN>& v, int k) const {
        return (cp[k] >= 0 ? v[cp[k]] : 0.0) - (cq[k] >= 0 ? v[cq[k]] : 0.0);
    }
    // W = Gi N (injection), K = M^T W (measurement)
    void reduce(const Mat11& Gi) {
        for (int i = 0; i < NN; ++i) {
            for (int k = 0; k < 4; ++k) Wm[i][k] = Gi[i][dA[k]] - Gi[i][dK[k]];
            Wm[i][4] = Gi[i][NB];
        }
        for (int k = 0; k < NP; ++k) {
            for (int j = 0; j < 4; ++j) K[j][k] = Wm[dA[j]][k] - Wm[dK[j]][k];
            K[4][k] = Wm[P][k];
        }
    }
    // linear maps from z = [1, vs, i1, i2, b0..b5] to the port inputs and cap voltages
    void precompute(const Mat11& Gi) {
        for (int k = 0; k < NZ; ++k) {
            std::array<double, NN> r{};
            if (k == 0) r = rhs;
            else if (k == 1) r = src;
            else if (k == 2) r[L] = -1.0;
            else if (k == 3) r[R] = -1.0;
            else {
                int c = k - 4;
                if (cp[c] >= 0) r[cp[c]] += 1.0 / Rc[c];
                if (cq[c] >= 0) r[cq[c]] -= 1.0 / Rc[c];
            }
            auto col = mul(Gi, r);
            auto u = measure(col);
            for (int j = 0; j < NP; ++j) Au[j][k] = u[j];
            for (int c = 0; c < NC; ++c) Ac[c][k] = port(col, c);
        }
        for (int c = 0; c < NC; ++c)
            for (int j = 0; j < NP; ++j)
                Wc[c][j] = (cp[c] >= 0 ? Wm[cp[c]][j] : 0.0) - (cq[c] >= 0 ? Wm[cq[c]][j] : 0.0);
    }

    void currents(const std::array<double, NP>& u, const Controls& c,
                  std::array<double, NP>& cur, std::array<double, NP>& der) const {
        for (int k = 0; k < 4; ++k) {
            double nvt = dMult[k] * N_D * VT, x = u[k] / nvt;
            if (x > 40.0) { double e = std::exp(40.0); cur[k] = IS_D * (e * (1.0 + x - 40.0) - 1.0); der[k] = IS_D * e / nvt; }
            else          { double e = std::exp(x);    cur[k] = IS_D * (e - 1.0);                  der[k] = IS_D * e / nvt; }
        }
        double d;
        double y = railClip(GAIN1 * u[4], d);
        cur[4] = -c.kfb * y / R15;
        der[4] = -c.kfb * GAIN1 * d / R15;
    }

    // residual F(u) = u - u0 + K i(u); also returns the currents' derivatives
    double residual(const std::array<double, NP>& u, const std::array<double, NP>& u0, const Controls& c,
                    std::array<double, NP>& F, std::array<double, NP>& der) const {
        std::array<double, NP> cur;
        currents(u, c, cur, der);
        double n2 = 0;
        for (int i = 0; i < NP; ++i) {
            double acc = u[i] - u0[i];
            for (int j = 0; j < NP; ++j) acc += K[i][j] * cur[j];
            F[i] = acc; n2 += acc * acc;
        }
        return n2;
    }

    // SPICE-style junction limiting (pnjlim) for one merged diode group
    static double pnjlim(double vnew, double vold, double nvt) {
        const double vcrit = nvt * std::log(nvt / (1.41421356 * IS_D));
        if (vnew > vcrit && std::abs(vnew - vold) > 2.0 * nvt) {
            if (vold > 0) {
                double arg = 1.0 + (vnew - vold) / nvt;
                vnew = arg > 0 ? vold + nvt * std::log(arg) : vcrit;
            } else {
                vnew = nvt * std::log(vnew / nvt);
            }
        }
        return vnew;
    }

    // Newton with per-junction limiting on the 4 diode ports; the op-amp
    // port (vP) takes full steps.
    int newton(std::array<double, NP>& u, const std::array<double, NP>& u0, const Controls& c,
               int maxit, double tol) const {
        std::array<double, NP> F, der;
        int it = 0;
        for (it = 1; it <= maxit; ++it) {
            residual(u, u0, c, F, der);
            double J[NP][NP], rhsF[NP];
            for (int i = 0; i < NP; ++i) {
                rhsF[i] = F[i];
                for (int j = 0; j < NP; ++j) J[i][j] = K[i][j] * der[j] + (i == j ? 1.0 : 0.0);
            }
            for (int k = 0; k < NP; ++k) {           // elimination, partial pivoting
                int p = k;
                for (int i = k + 1; i < NP; ++i) if (std::abs(J[i][k]) > std::abs(J[p][k])) p = i;
                if (p != k) { for (int j = 0; j < NP; ++j) std::swap(J[k][j], J[p][j]); std::swap(rhsF[k], rhsF[p]); }
                for (int i = k + 1; i < NP; ++i) {
                    double f = J[i][k] / J[k][k];
                    for (int j = k; j < NP; ++j) J[i][j] -= f * J[k][j];
                    rhsF[i] -= f * rhsF[k];
                }
            }
            double X[NP];
            for (int i = NP - 1; i >= 0; --i) {
                double s = rhsF[i];
                for (int j = i + 1; j < NP; ++j) s -= J[i][j] * X[j];
                X[i] = s / J[i][i];
            }
            double mx = 0;
            for (int i = 0; i < NP; ++i) {
                double unew = u[i] - X[i];
                if (i < 4) unew = pnjlim(unew, u[i], dMult[i] * N_D * VT);
                mx = std::max(mx, std::abs(unew - u[i]));
                u[i] = unew;
            }
            if (mx < tol) break;
        }
        return it;
    }
};

} // namespace sp
