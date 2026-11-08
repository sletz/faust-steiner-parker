#!/usr/bin/env python3
"""Checks of faust-steiner-parker against the original C++ model.

Every comparison drives the original model (ref/SPModel.h, unchanged, through
ref/sp_ref.cpp) and the Faust programs with the same input and settings, and
compares the outputs sample by sample. Both forms of the Newton loop are
checked: `unrolled` (dsp/steinerparker*.dsp, any Faust compiler) and
`repeat` (dsp/steinerparker_rs*.dsp, faust-rs: the loop exits as the C++
does). Writes results/results.md and results/results.json; exits 1 when a
check fails.

  python3 scripts/test_sp.py [--quick] [--no-time]

The web check needs node and the wasm of `make web`.

Environment: FAUST_RS (a faust-rs checkout with release faustprobe and
faust-rs built, default ../faust-rs), FAUSTPROBE and FAUSTRS (default: the
binaries of that checkout), FAUST (the C++ Faust compiler, default: faust on
the PATH), CXX (default: c++).
"""
import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FAUST_RS = os.environ.get("FAUST_RS", os.path.join(os.path.dirname(ROOT), "faust-rs"))
FAUSTPROBE = os.environ.get("FAUSTPROBE", os.path.join(FAUST_RS, "target", "release", "faustprobe"))
FAUSTRS = os.environ.get("FAUSTRS", os.path.join(FAUST_RS, "target", "release", "faust-rs"))
FAUST = os.environ.get("FAUST", "faust")
CXX = os.environ.get("CXX", "c++")
BUILD = os.path.join(ROOT, "build")
DSP = os.path.join(ROOT, "dsp")
REF = os.path.join(BUILD, "sp_ref")
ARCH = os.path.join(ROOT, "tools", "bench_arch.cpp")
CXXFLAGS = ["-std=c++17", "-O3", "-ffp-contract=off"]
FORMS = ("repeat", "unrolled")

# Tolerances, in the units of each output, measured against max(peak, 0.1) of
# the reference. The DC operating point of the two implementations differs by
# about 1e-9 V (its matrix spans 1e-12 to 1e-3 S), which bounds the agreement.
CORE_TOL = 5e-7          # Vo1, volts (peak about 1 V), over 0.125 s at 192 kHz: the two implementations
                         # settle on DC offsets 1.6e-7 to 1.8e-7 V apart (README, section 3), still growing here
PLUGIN_TOL_DB = -110.0   # plugin output, max error
STRESS_TOL_DB = -70.0    # full resonance with +24 dB of drive: one-sample spikes at the clipped edges
STRESS_RMS_DB = -100.0   # ... and the RMS error, which these spikes do not move
CXX_TOL_DB = -100.0      # a C++ backend against the Cranelift JIT: two compilers, two operation orders
PERTURB = 1e-9           # relative input perturbations of the C++ (k = 1..4): the size of the difference the two
                         # implementations start with (their DC points differ by about 1e-9 V)

PRESETS = [  # name, jack, cut, res, cv, drive, out, quality, t1 (plugin/Source/PresetManager.h)
    ("Init", 0, 0.51, 0.40, 0.0, 0.0, -6.0, 2, 0.45),
    ("Warm Low-pass", 0, 0.46, 0.30, 0.0, 3.0, -6.0, 2, 0.45),
    ("Acid Squelch", 0, 0.49, 0.85, 0.0, 9.0, -9.0, 2, 0.45),
    ("Vocal Band", 1, 0.53, 0.70, 0.0, 0.0, -3.0, 2, 0.45),
    ("Thin High-pass", 2, 0.55, 0.30, 0.0, 0.0, -6.0, 2, 0.45),
    ("Bright Air", 2, 0.69, 0.55, 0.0, -6.0, -3.0, 2, 0.45),
    ("Dark Drone", 0, 0.32, 0.60, 0.0, 12.0, -6.0, 2, 0.45),
    ("Self-oscillation", 0, 0.44, 0.97, 0.0, 0.0, -12.0, 4, 0.45),
]

TMP = None


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{r.stdout}\n{r.stderr}")
    return r


def build_ref():
    os.makedirs(BUILD, exist_ok=True)
    run([CXX, *CXXFLAGS, "-o", REF, os.path.join(ROOT, "ref", "sp_ref.cpp")])


def program(quality, form):
    base = "steinerparker_rs" if form == "repeat" else "steinerparker"
    return os.path.join(DSP, base + (".dsp" if quality == 2 else f"_{quality}x.dsp"))


def faustprobe(dsp, sr, n, infile, sets):
    out = os.path.join(TMP, "fp.npy")
    cmd = [FAUSTPROBE, "--double", "-I", DSP, "--sr", str(sr), "-n", str(n),
           "--in", f"file:{infile}", "--quiet", "--out", out]
    for k, v in sets:
        cmd += ["--set", f"{k}={v}"]
    run(cmd + [dsp])
    return np.load(out)


def newton_stats(stderr):
    for line in stderr.splitlines():
        if line.startswith("newton:"):
            return line[len("newton: "):]
    return ""


def saw(fs, n, hz, amp):
    ph = np.fmod(np.arange(n) * hz / fs, 1.0)
    return amp * (2.0 * ph - 1.0)


def write_f64(x, name):
    p = os.path.join(TMP, name)
    np.asarray(x, dtype="<f8").tofile(p)
    return p


def db(x, peak):
    return float(20 * np.log10(x / max(peak, 0.1))) if x > 0 else float("-inf")


def compare(ref, fa):
    e = np.abs(ref - fa)
    peak = float(np.abs(ref).max())
    return {"frames": int(len(ref)), "peak": peak, "max_err": float(e.max()), "at": int(e.argmax()),
            "db": db(float(e.max()), peak), "rms_db": db(float(np.sqrt(np.mean(e ** 2))), peak),
            "finite": bool(np.isfinite(fa).all())}


def core_checks(quick):
    """The circuit alone, as `model_test` of the original: a 110 Hz saw of 5 V
    at 192 kHz, CUT 0.6, RES pot 0.6, T1 0.5, the three jacks."""
    fs, n = 192000, 6000 if quick else 24000
    ph = np.arange(n) * 110.0 / fs
    inp = write_f64(-(5.0 * 2.0 * (ph - np.floor(ph + 0.5))), "core_in.f64")
    rows = []
    for jack in range(3):
        refout = os.path.join(TMP, "core_ref.f64")
        r = run([REF, "core", str(jack), str(fs), "0.6", "0", "0.6", "0.5", inp, refout])
        ref = np.fromfile(refout, "<f8")
        fa = faustprobe(os.path.join(DSP, "sp_core.dsp"), fs, n, inp, [("jack", jack)])[:, 0]
        c = compare(ref, fa)
        c.update(jack="ABC"[jack], newton=newton_stats(r.stderr), ok=c["finite"] and c["max_err"] < CORE_TOL)
        rows.append(c)
    return rows


def descriptors(y, fs):
    """Level (dB RMS) and spectral centroid (Hz) of the second half."""
    y = y[len(y) // 2:]
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1.0 / fs)
    return float(10 * np.log10(np.mean(y ** 2))), float((f * spec).sum() / spec.sum())


def ref_plugin(jack, fs, quality, cut, cv, res, t1, drive, out, inp):
    refout = os.path.join(TMP, "plug_ref.f64")
    r = run([REF, "plugin", str(jack), str(fs), str(quality), str(cut), str(cv), str(res), str(t1),
             str(drive), str(out), inp, refout])
    return np.fromfile(refout, "<f8"), r.stderr


def plugin_case(name, jack, cut, res, cv, drive, out, quality, t1, n, tol_db=PLUGIN_TOL_DB, fs=48000, hz=55.0, amp=0.9):
    x = saw(fs, n, hz, amp)
    inp = write_f64(x, "plug_in.f64")
    ref, err = ref_plugin(jack, fs, quality, cut, cv, res, t1, drive, out, inp)
    sets = [("Response", jack), ("Frequency", cut), ("Resonance", res), ("CV_offset", cv),
            ("Input_level", drive), ("Output", out), ("V-oct_trim", t1)]
    c = {"name": name, "quality": quality, "newton": newton_stats(err), "tol_db": tol_db,
         "peak": float(np.abs(ref).max()), "frames": n}
    # How much does the C++ itself move when its input moves by k PERTURB?
    perturbed = [ref_plugin(jack, fs, quality, cut, cv, res, t1, drive, out,
                            write_f64(x * (1 + k * PERTURB), "plug_inp.f64"))[0] for k in range(1, 5)]
    c["self_db"] = db(max(float(np.abs(ref - rp).max()) for rp in perturbed), c["peak"])
    # a sample tolerance the original cannot meet against itself is not applied
    chaotic = c["self_db"] > tol_db
    c["mode"] = "descriptors" if chaotic else "samples"
    outs = {}
    ok = True
    for form in FORMS:
        fa = faustprobe(program(quality, form), fs, n, inp, sets)
        outs[form] = fa
        r = compare(ref, fa[:, 0])
        r["l_r"] = float(np.abs(fa[:, 0] - fa[:, 1]).max())
        if chaotic:
            # compare what a listener hears, against the spread of the C++ itself
            d0 = descriptors(ref, fs)
            dp = [descriptors(rp, fs) for rp in perturbed]
            df = [descriptors(fa[:, ch], fs) for ch in (0, 1)]
            spread = [max(abs(d[i] - d0[i]) for d in dp) for i in (0, 1)]
            bound = [max(3 * spread[0], 0.05), max(3 * spread[1], 0.005 * d0[1])]
            dev = [max(abs(d[i] - d0[i]) for d in df) for i in (0, 1)]
            r.update(ref_desc=d0, faust_desc=df, spread=spread, bound=bound, dev=dev,
                     ok=r["finite"] and dev[0] <= bound[0] and dev[1] <= bound[1])
        else:
            r["ok"] = r["finite"] and r["db"] < tol_db and (tol_db == PLUGIN_TOL_DB or r["rms_db"] < STRESS_RMS_DB)
        c[form] = r
        ok = ok and r["ok"]
    c["forms_db"] = db(float(np.abs(outs["repeat"] - outs["unrolled"]).max()), c["peak"])
    c["ok"] = ok
    return c


def plugin_checks(quick):
    n = 24000 if quick else 96000
    rows = [plugin_case(f"default {q}x", 0, 0.51, 0.4, 0.0, 0.0, -6.0, q, 0.45, n) for q in (1, 2, 4)]
    rows.append(plugin_case("BP res 0.7 4x", 1, 0.6, 0.7, 0.0, 0.0, -6.0, 4, 0.45, n))
    rows.append(plugin_case("HP 1x", 2, 0.5, 0.4, 0.0, 0.0, -6.0, 1, 0.45, n))
    rows.append(plugin_case("LP closed", 0, 0.0, 0.5, 0.0, 0.0, -6.0, 2, 0.45, n))
    rows.append(plugin_case("CV -3 V, trim 0.225", 0, 0.7, 0.5, -3.0, 0.0, -6.0, 2, 0.225, n))
    rows.append(plugin_case("scream 2x (res 1, +24 dB)", 0, 0.55, 1.0, 0.0, 24.0, -6.0, 2, 0.45, n, STRESS_TOL_DB))
    rows.append(plugin_case("scream 4x (res 1, +24 dB)", 0, 0.55, 1.0, 0.0, 24.0, -6.0, 4, 0.45, n, STRESS_TOL_DB))
    rows.append(plugin_case("max cut, res 1, +24 dB 4x", 0, 1.0, 1.0, 0.0, 24.0, -6.0, 4, 0.45, n, STRESS_TOL_DB))
    return rows


def preset_checks(quick):
    n = 24000 if quick else 96000
    return [plugin_case(*p, n) for p in PRESETS]


def build_bench(compiler, quality, form):
    """A benchmark/render executable from the C++ that `compiler` generates."""
    tag = "faust" if compiler == FAUST else "faustrs"
    exe = os.path.join(BUILD, f"bench_{tag}_{form}_{quality}x")
    if compiler == FAUST:
        run([FAUST, "-double", "-I", DSP, "-a", ARCH, program(quality, form), "-o", exe + ".cpp"])
    else:
        run([FAUSTRS, "-lang", "cpp", "-double", "-I", DSP, "-a", ARCH, program(quality, form), "-o", exe + ".cpp"])
    run([CXX, *CXXFLAGS, "-I/usr/local/include", "-o", exe, exe + ".cpp"])
    return exe


def cxx_backend_checks():
    """The C++ that each compiler generates renders what faustprobe renders."""
    n = 48000
    inp = write_f64(saw(48000, n, 55.0, 0.9), "cxx_in.f64")
    rows = []
    for compiler, form, label in ((FAUST, "unrolled", "faust (C++), unrolled"),
                                  (FAUSTRS, "unrolled", "faust-rs -lang cpp, unrolled"),
                                  (FAUSTRS, "repeat", "faust-rs -lang cpp, repeat")):
        exe = build_bench(compiler, 2, form)
        outp = os.path.join(TMP, "cxx_out.f64")
        run([exe, "render", "48000", inp, outp, "Resonance=0.8", "Input_level=6"])
        fa = faustprobe(program(2, form), 48000, n, inp, [("Resonance", 0.8), ("Input_level", 6)])[:, 0]
        c = compare(fa, np.fromfile(outp, "<f8"))
        c.update(label=label, ok=c["finite"] and c["db"] < CXX_TOL_DB)
        rows.append(c)
    return rows


def web_checks():
    """The wasm of the web version (web/dsp, faust-rs -lang wasm -double), run under
    node through the page's own loader, renders what faustprobe renders."""
    n = 48000
    inp = write_f64(saw(48000, n, 55.0, 0.9), "web_in.f64")
    rows = []
    for q in (1, 2, 4):
        outp = os.path.join(TMP, "web_out.f64")
        run(["node", os.path.join(ROOT, "scripts", "web_check.mjs"), "render", str(q), inp, outp,
             "Filter/Resonance=0.8", "Signal/Input_level=6"])
        fa = faustprobe(program(q, "repeat"), 48000, n, inp, [("Resonance", 0.8), ("Input_level", 6)])[:, 0]
        c = compare(fa, np.fromfile(outp, "<f8"))
        c.update(label=f"faust-rs -lang wasm -double (web/dsp/sp{q}x.wasm), node, repeat, {q}x", ok=c["finite"] and c["db"] < CXX_TOL_DB)
        rows.append(c)
    return rows


def realtime(line):
    m = re.search(r"([0-9.]+)x real time", line)
    return float(m.group(1)) if m else float("nan")


def best_of(cmd, runs=3):
    """The fastest of `runs` timings (the others measure the machine's noise)."""
    outs = [run(cmd) for _ in range(runs)]
    lines = [(o.stdout + o.stderr) for o in outs]
    return max(lines, key=lambda t: realtime(t)), outs[0]


def timing(seconds=5.0):
    """Best of three runs of each implementation."""
    rows = []
    for q in (1, 2, 4):
        text, first = best_of([REF, "time", "0", "48000", str(q), str(seconds)])
        rows.append({"impl": "C++ original (SPModel.h)", "quality": q, "line": first_line(text, "real time"),
                     "newton": newton_stats(first.stderr)})
        for compiler, form, label in ((FAUSTRS, "repeat", "Faust, faust-rs -lang cpp, repeat"),
                                      (FAUSTRS, "unrolled", "Faust, faust-rs -lang cpp, unrolled (12)"),
                                      (FAUST, "unrolled", "Faust, faust (C++) -lang cpp, unrolled (12)")):
            text, _ = best_of([build_bench(compiler, q, form), "time", "48000", str(seconds)])
            rows.append({"impl": label, "quality": q, "line": first_line(text, "real time")})
        text, _ = best_of(["node", os.path.join(ROOT, "scripts", "web_check.mjs"), "time", str(q), str(seconds)])
        rows.append({"impl": "Faust, faust-rs -lang wasm, repeat, under node", "quality": q, "line": first_line(text, "real time")})
        for form in FORMS:
            text, _ = best_of([FAUSTPROBE, "--double", "-I", DSP, "--sr", "48000", "-n", str(int(48000 * seconds)),
                               "--in", "sine:55", "--quiet", "--time", program(q, form)])
            rows.append({"impl": f"Faust, faustprobe (Cranelift JIT), {form}", "quality": q,
                         "line": first_line(text, "time: compute").lstrip("# ")})
    return rows


def first_line(text, needle):
    return next((l for l in text.splitlines() if needle in l), "").strip()


def fmt_rows(rows, keys):
    out = ["| " + " | ".join(h for h, _ in keys) + " |", "|" + "---|" * len(keys)]
    for r in rows:
        out.append("| " + " | ".join(f(r) for _, f in keys) + " |")
    return "\n".join(out)


def yes(b):
    return "yes" if b else "**NO**"


def main():
    global TMP
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--no-time", action="store_true")
    ap.add_argument("--time-only", action="store_true", help="the CPU table alone, printed")
    a = ap.parse_args()
    TMP = tempfile.mkdtemp(prefix="sp_test_")
    if a.time_only:
        try:
            build_ref()
            rows = timing()
        finally:
            shutil.rmtree(TMP, ignore_errors=True)
        print(fmt_rows(rows, [("quality", lambda r: f"{r['quality']}x"), ("implementation", lambda r: r["impl"]),
                              ("real-time factor", lambda r: f"{realtime(r['line']):.1f}x")]))
        return 0
    try:
        build_ref()
        versions = {
            "faustprobe": f"{FAUSTPROBE}: " + run([FAUSTPROBE, "-v"]).stdout.splitlines()[0],
            "faust-rs": f"{FAUSTRS}: " + run([FAUSTRS, "--version"]).stdout.splitlines()[0],
            "faust": " / ".join(run([FAUST, "--version"]).stdout.splitlines()[:2]),
            "faust-rs commit": run(["git", "-C", FAUST_RS, "log", "-1", "--format=%h %ad %s", "--date=short"]).stdout.strip()
            + (" (with uncommitted changes)" if run(["git", "-C", FAUST_RS, "status", "--porcelain", "--untracked-files=no"]).stdout.strip() else ""),
            "C++ compiler": run([CXX, "--version"]).stdout.splitlines()[0] + f", flags `{' '.join(CXXFLAGS)}`",
        }
        core = core_checks(a.quick)
        plug = plugin_checks(a.quick)
        pres = preset_checks(a.quick)
        cxxb = cxx_backend_checks() + web_checks()
        versions["node"] = run(["node", "--version"]).stdout.strip()
        times = [] if a.no_time else timing()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)

    machine = f"{platform.machine()}, {platform.system()} {platform.release()}"
    try:
        machine = run(["sysctl", "-n", "machdep.cpu.brand_string"]).stdout.strip() + ", " + machine
    except Exception:
        pass
    ok = all(r["ok"] for r in core + plug + pres + cxxb)

    def form_cols(form):
        return [(f"{form}: max error", lambda r: f"{r[form]['max_err']:.2e} ({r[form]['db']:.0f} dB)"),
                (f"{form}: RMS", lambda r: f"{r[form]['rms_db']:.0f} dB")]

    md = ["# Results", "",
          "Written by `scripts/test_sp.py`" + (" --quick" if a.quick else "") + ". Do not edit.", ""]
    md += [f"- {k}: {v}" for k, v in versions.items()]
    md += [f"- machine: {machine}", f"- verdict: **{'PASS' if ok else 'FAIL'}**", "",
           "## Circuit alone (Vo1, volts)", "",
           "110 Hz saw of 5 V at 192 kHz, CUT 0.6, RES pot 0.6, T1 0.5, as `model_test` of the original "
           f"(`dsp/sp_core.dsp`, unrolled Newton); tolerance {CORE_TOL:g} V.", "",
           fmt_rows(core, [("jack", lambda r: r["jack"]), ("frames", lambda r: str(r["frames"])),
                           ("peak (V)", lambda r: f"{r['peak']:.4f}"), ("max error (V)", lambda r: f"{r['max_err']:.2e}"),
                           ("Newton in the C++", lambda r: r["newton"]), ("ok", lambda r: yes(r["ok"]))]),
           "", "## Plugin chain", "",
           "55 Hz saw of 0.9 at 48 kHz (1.0 = 5 V), smoothing, oversampling with the JSFX's filters, output stage, "
           "both forms of the Newton loop (`repeat`: dsp/steinerparker_rs*.dsp; `unrolled`: dsp/steinerparker*.dsp). "
           f"Errors in dB of max(peak, 0.1); tolerance {PLUGIN_TOL_DB:g} dB ({STRESS_TOL_DB:g} dB, and {STRESS_RMS_DB:g} dB RMS, "
           "for full resonance with +24 dB of drive, where the error comes as one-sample spikes at the edges of the clipped "
           "output). `C++ vs C++`: how far the original moves when its input is scaled by 1 + k 1e-9 (k = 1..4), the size of "
           "the difference the two implementations start with. "
           "`forms`: the largest difference between the two Faust forms.", "",
           fmt_rows(plug + pres, [("setting", lambda r: r["name"]), ("quality", lambda r: f"{r['quality']}x"),
                                  ("frames", lambda r: str(r["frames"])), ("peak", lambda r: f"{r['peak']:.4f}"),
                                  *form_cols("repeat"), *form_cols("unrolled"),
                                  ("forms", lambda r: f"{r['forms_db']:.0f} dB"),
                                  ("C++ vs C++", lambda r: f"{r['self_db']:.0f} dB"),
                                  ("compared by", lambda r: (f"samples, {r['tol_db']:.0f} dB" if r["mode"] == "samples" else "descriptors")),
                                  ("Newton in the C++", lambda r: r["newton"]), ("ok", lambda r: yes(r["ok"]))]),
           "", "Where the C++ itself moves under these perturbations by more than the tolerance, the regime is chaotic "
           "(a self-oscillation that starts from rounding noise, or a screaming setting whose differences grow): "
           "the implementations are compared by their level (dB RMS) and "
           "spectral centroid over the second half, and each Faust output must stay within three times the spread of "
           "the perturbed C++ runs (at least 0.05 dB and 0.5 %).", ""]
    chaotic = [r for r in plug + pres if r["mode"] == "descriptors"]
    if chaotic:
        rows = [dict(r[f], name=r["name"], form=f) for r in chaotic for f in FORMS]
        md += [fmt_rows(rows, [("setting", lambda r: r["name"]), ("form", lambda r: r["form"]),
                               ("C++ level, centroid", lambda r: f"{r['ref_desc'][0]:.3f} dB, {r['ref_desc'][1]:.1f} Hz"),
                               ("Faust L / R level", lambda r: f"{r['faust_desc'][0][0]:.3f} / {r['faust_desc'][1][0]:.3f} dB"),
                               ("Faust L / R centroid", lambda r: f"{r['faust_desc'][0][1]:.1f} / {r['faust_desc'][1][1]:.1f} Hz"),
                               ("deviation (bound)", lambda r: f"{r['dev'][0]:.3f} dB ({r['bound'][0]:.3f}), {r['dev'][1]:.1f} Hz ({r['bound'][1]:.1f})"),
                               ("L-R", lambda r: f"{r['l_r']:.2f}"), ("ok", lambda r: yes(r["ok"]))]), ""]
    md += ["## Other backends against faustprobe", "",
           f"The generated C++ compiled with `{CXX} {' '.join(CXXFLAGS)}`, and the wasm of the web version run under node "
           "by the page's own loader, against the Cranelift JIT of faustprobe: Resonance 0.8, Input level +6 dB, "
           f"48000 frames; tolerance {CXX_TOL_DB:g} dB.", "",
           fmt_rows(cxxb, [("compiler, form", lambda r: r["label"]), ("max difference", lambda r: f"{r['max_err']:.2e} ({r['db']:.0f} dB)"),
                           ("ok", lambda r: yes(r["ok"]))]), ""]
    if times:
        md += ["## CPU cost", "",
               "Stereo, 48 kHz, 5 s of a 55 Hz saw (0.9 left, -0.9 right), default settings, the compute call alone timed, best of three runs "
               "(faustprobe: a 55 Hz sine, its own timer). The C++ programs are built with the same compiler and flags.", "",
               fmt_rows(times, [("quality", lambda r: f"{r['quality']}x"), ("implementation", lambda r: r["impl"]),
                                ("real-time factor", lambda r: f"{realtime(r['line']):.1f}x"),
                                ("output", lambda r: f"`{r['line']}`")]), ""]
    os.makedirs(os.path.join(ROOT, "results"), exist_ok=True)
    with open(os.path.join(ROOT, "results", "results.md"), "w") as f:
        f.write("\n".join(md))
    with open(os.path.join(ROOT, "results", "results.json"), "w") as f:
        json.dump({"ok": ok, "versions": versions, "machine": machine, "core": core, "plugin": plug,
                   "presets": pres, "cxx_backends": cxxb, "timing": times}, f, indent=1)
    print("\n".join(md))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
