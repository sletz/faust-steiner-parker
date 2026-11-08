# Results

Written by `scripts/test_sp.py`. Do not edit.

- faustprobe: /Users/letz/Developpements/RUST/faust-rs/target/release/faustprobe: faustprobe 0.9.5
- faust-rs: /Users/letz/Developpements/RUST/faust-rs/target/release/faust-rs: faust-rs 0.9.5 (2ecf66f6 2026-11-08)
- faust: FAUST Version 2.92.5 / Source commit: f8d9a8d7e
- faust-rs commit: 2ecf66f6 2026-11-08 impulse-tests: the recorded-bell fit timed against the CPU (with uncommitted changes)
- C++ compiler: Apple clang version 21.0.0 (clang-2100.4.1.1), flags `-std=c++17 -O3 -ffp-contract=off`
- node: v22.12.0
- machine: Apple M5, arm64, Darwin 25.6.0
- verdict: **PASS**

## Circuit alone (Vo1, volts)

110 Hz saw of 5 V at 192 kHz, CUT 0.6, RES pot 0.6, T1 0.5, as `model_test` of the original (`dsp/sp_core.dsp`, unrolled Newton); tolerance 5e-07 V.

| jack | frames | peak (V) | max error (V) | Newton in the C++ | ok |
|---|---|---|---|---|---|
| A | 24000 | 0.6309 | 4.90e-08 | 24000 ticks, mean 2.123 iterations, max 5; histogram 2:21400 3:2250 4:336 5:14 | yes |
| B | 24000 | 0.8359 | 5.18e-08 | 24000 ticks, mean 2.118 iterations, max 5; histogram 2:21638 3:1914 4:434 5:14 | yes |
| C | 24000 | 0.9044 | 1.94e-07 | 24000 ticks, mean 1.472 iterations, max 6; histogram 1:15886 2:5359 3:2321 4:420 6:14 | yes |

## Plugin chain

55 Hz saw of 0.9 at 48 kHz (1.0 = 5 V), smoothing, oversampling with the JSFX's filters, output stage, both forms of the Newton loop (`repeat`: dsp/steinerparker_rs*.dsp; `unrolled`: dsp/steinerparker*.dsp). Errors in dB of max(peak, 0.1); tolerance -110 dB (-70 dB, and -100 dB RMS, for full resonance with +24 dB of drive, where the error comes as one-sample spikes at the edges of the clipped output). `C++ vs C++`: how far the original moves when its input is scaled by 1 + k 1e-9 (k = 1..4), the size of the difference the two implementations start with. `forms`: the largest difference between the two Faust forms.

| setting | quality | frames | peak | repeat: max error | repeat: RMS | unrolled: max error | unrolled: RMS | forms | C++ vs C++ | compared by | Newton in the C++ | ok |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| default 1x | 1x | 96000 | 0.6373 | 2.47e-07 (-128 dB) | -132 dB | 2.47e-07 (-128 dB) | -132 dB | -260 dB | -168 dB | samples, -110 dB | 96000 ticks, mean 2.059 iterations, max 5; histogram 2:91053 3:4291 4:547 5:109 | yes |
| default 2x | 2x | 96000 | 0.6369 | 5.85e-07 (-121 dB) | -124 dB | 5.85e-07 (-121 dB) | -124 dB | -255 dB | -168 dB | samples, -110 dB | 192000 ticks, mean 2.047 iterations, max 4; histogram 2:183647 3:7587 4:766 | yes |
| default 4x | 4x | 96000 | 0.6368 | 1.19e-06 (-115 dB) | -118 dB | 1.19e-06 (-115 dB) | -118 dB | -252 dB | -168 dB | samples, -110 dB | 384000 ticks, mean 2.944 iterations, max 4; histogram 2:22450 3:360784 4:766 | yes |
| BP res 0.7 4x | 4x | 96000 | 1.0459 | 1.20e-06 (-119 dB) | -122 dB | 1.20e-06 (-119 dB) | -122 dB | -253 dB | -171 dB | samples, -110 dB | 384000 ticks, mean 2.965 iterations, max 4; histogram 2:15950 3:365427 4:2623 | yes |
| HP 1x | 1x | 96000 | 1.0274 | 2.77e-07 (-131 dB) | -135 dB | 2.77e-07 (-131 dB) | -135 dB | -264 dB | -172 dB | samples, -110 dB | 96000 ticks, mean 2.066 iterations, max 5; histogram 2:90503 3:4732 4:656 5:109 | yes |
| LP closed | 2x | 96000 | 0.0036 | 5.16e-08 (-126 dB) | -130 dB | 5.16e-08 (-126 dB) | -130 dB | -238 dB | -196 dB | samples, -110 dB | 192000 ticks, mean 2.007 iterations, max 3; histogram 2:190681 3:1319 | yes |
| CV -3 V, trim 0.225 | 2x | 96000 | 0.2262 | 3.88e-07 (-115 dB) | -120 dB | 3.88e-07 (-115 dB) | -120 dB | -241 dB | -168 dB | samples, -110 dB | 192000 ticks, mean 2.118 iterations, max 3; histogram 2:169339 3:22661 | yes |
| scream 2x (res 1, +24 dB) | 2x | 96000 | 1.0525 | 1.07e+00 (0 dB) | -34 dB | 1.07e+00 (0 dB) | -34 dB | -135 dB | -17 dB | descriptors | 192000 ticks, mean 5.288 iterations, max 13; histogram 4:41353 5:82646 6:45673 7:16508 8:5547 9:123 10:89 11:27 12:24 13:10 | yes |
| scream 4x (res 1, +24 dB) | 4x | 96000 | 1.0525 | 1.00e-01 (-20 dB) | -67 dB | 1.00e-01 (-20 dB) | -67 dB | -152 dB | -55 dB | descriptors | 384000 ticks, mean 4.608 iterations, max 10; histogram 3:1611 4:174105 5:182369 6:25250 7:378 8:210 9:75 10:2 | yes |
| max cut, res 1, +24 dB 4x | 4x | 96000 | 1.0525 | 1.17e-06 (-119 dB) | -133 dB | 1.17e-06 (-119 dB) | -133 dB | -257 dB | -171 dB | samples, -70 dB | 384000 ticks, mean 3.873 iterations, max 8; histogram 3:55817 4:323022 5:3739 6:984 7:329 8:109 | yes |
| Init | 2x | 96000 | 0.6369 | 5.85e-07 (-121 dB) | -124 dB | 5.85e-07 (-121 dB) | -124 dB | -255 dB | -168 dB | samples, -110 dB | 192000 ticks, mean 2.047 iterations, max 4; histogram 2:183647 3:7587 4:766 | yes |
| Warm Low-pass | 2x | 96000 | 0.8339 | 5.86e-07 (-123 dB) | -127 dB | 5.86e-07 (-123 dB) | -127 dB | -259 dB | -169 dB | samples, -110 dB | 192000 ticks, mean 2.062 iterations, max 4; histogram 2:180899 3:10225 4:876 | yes |
| Acid Squelch | 2x | 96000 | 0.7451 | 4.38e-07 (-125 dB) | -131 dB | 4.38e-07 (-125 dB) | -131 dB | -246 dB | -157 dB | samples, -110 dB | 192000 ticks, mean 2.529 iterations, max 5; histogram 2:95453 3:91953 4:4158 5:436 | yes |
| Vocal Band | 2x | 96000 | 1.4760 | 8.42e-07 (-125 dB) | -128 dB | 8.42e-07 (-125 dB) | -128 dB | -254 dB | -169 dB | samples, -110 dB | 192000 ticks, mean 2.106 iterations, max 4; histogram 2:173755 3:16169 4:2076 | yes |
| Thin High-pass | 2x | 96000 | 1.0377 | 7.30e-07 (-123 dB) | -127 dB | 7.30e-07 (-123 dB) | -127 dB | -262 dB | -172 dB | samples, -110 dB | 192000 ticks, mean 2.029 iterations, max 4; histogram 2:187162 3:4181 4:657 | yes |
| Bright Air | 2x | 96000 | 0.9064 | 1.03e-06 (-119 dB) | -123 dB | 1.03e-06 (-119 dB) | -123 dB | -256 dB | -168 dB | samples, -110 dB | 192000 ticks, mean 1.244 iterations, max 4; histogram 1:149570 2:38803 3:2751 4:876 | yes |
| Dark Drone | 2x | 96000 | 1.0518 | 6.77e-07 (-124 dB) | -133 dB | 6.77e-07 (-124 dB) | -133 dB | -253 dB | -159 dB | samples, -110 dB | 192000 ticks, mean 2.241 iterations, max 6; histogram 2:147356 3:43330 4:1096 5:109 6:109 | yes |
| Self-oscillation | 4x | 96000 | 0.5275 | 1.05e+00 (6 dB) | 1 dB | 1.05e+00 (6 dB) | 1 dB | 6 dB | 6 dB | descriptors | 384000 ticks, mean 3.250 iterations, max 4; histogram 3:287894 4:96106 | yes |

Where the C++ itself moves under these perturbations by more than the tolerance, the regime is chaotic (a self-oscillation that starts from rounding noise, or a screaming setting whose differences grow): the implementations are compared by their level (dB RMS) and spectral centroid over the second half, and each Faust output must stay within three times the spread of the perturbed C++ runs (at least 0.05 dB and 0.5 %).

| setting | form | C++ level, centroid | Faust L / R level | Faust L / R centroid | deviation (bound) | L-R | ok |
|---|---|---|---|---|---|---|---|
| scream 2x (res 1, +24 dB) | repeat | 0.118 dB, 3423.8 Hz | 0.118 / 0.118 dB | 3423.6 / 3423.6 Hz | 0.000 dB (0.050), 0.2 Hz (17.1) | 0.00 | yes |
| scream 2x (res 1, +24 dB) | unrolled | 0.118 dB, 3423.8 Hz | 0.118 / 0.118 dB | 3423.6 / 3423.6 Hz | 0.000 dB (0.050), 0.2 Hz (17.1) | 0.00 | yes |
| scream 4x (res 1, +24 dB) | repeat | 0.117 dB, 3357.0 Hz | 0.117 / 0.117 dB | 3357.0 / 3357.0 Hz | 0.000 dB (0.050), 0.0 Hz (16.8) | 0.00 | yes |
| scream 4x (res 1, +24 dB) | unrolled | 0.117 dB, 3357.0 Hz | 0.117 / 0.117 dB | 3357.0 / 3357.0 Hz | 0.000 dB (0.050), 0.0 Hz (16.8) | 0.00 | yes |
| Self-oscillation | repeat | -6.504 dB, 1461.7 Hz | -6.504 / -6.504 dB | 1464.0 / 1464.0 Hz | 0.000 dB (0.050), 2.3 Hz (27.8) | 0.00 | yes |
| Self-oscillation | unrolled | -6.504 dB, 1461.7 Hz | -6.505 / -6.502 dB | 1459.4 / 1464.9 Hz | 0.003 dB (0.050), 3.2 Hz (27.8) | 1.05 | yes |

## Other backends against faustprobe

The generated C++ compiled with `c++ -std=c++17 -O3 -ffp-contract=off`, and the wasm of the web version run under node by the page's own loader, against the Cranelift JIT of faustprobe: Resonance 0.8, Input level +6 dB, 48000 frames; tolerance -100 dB.

| compiler, form | max difference | ok |
|---|---|---|
| faust (C++), unrolled | 2.16e-06 (-114 dB) | yes |
| faust-rs -lang cpp, unrolled | 7.67e-07 (-123 dB) | yes |
| faust-rs -lang cpp, repeat | 7.67e-07 (-123 dB) | yes |
| faust-rs -lang wasm -double (web/dsp/sp1x.wasm), node, repeat, 1x | 1.49e-08 (-157 dB) | yes |
| faust-rs -lang wasm -double (web/dsp/sp2x.wasm), node, repeat, 2x | 1.51e-08 (-157 dB) | yes |
| faust-rs -lang wasm -double (web/dsp/sp4x.wasm), node, repeat, 4x | 1.51e-08 (-157 dB) | yes |

## CPU cost

Stereo, 48 kHz, 5 s of a 55 Hz saw (0.9 left, -0.9 right), default settings, the compute call alone timed, best of three runs (faustprobe: a 55 Hz sine, its own timer). The C++ programs are built with the same compiler and flags.

| quality | implementation | real-time factor | output |
|---|---|---|---|
| 1x | C++ original (SPModel.h) | 52.4x | `C++ original: 5.000 s of stereo audio at 48000 Hz x1 in 0.0955 s: 52.4x real time, 397.9 ns per stereo sample` |
| 1x | Faust, faust-rs -lang cpp, repeat | 53.7x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.0931 s: 53.7x real time, 387.9 ns per stereo sample` |
| 1x | Faust, faust-rs -lang cpp, unrolled (12) | 11.3x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.4433 s: 11.3x real time, 1847.2 ns per stereo sample` |
| 1x | Faust, faust (C++) -lang cpp, unrolled (12) | 11.2x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.4482 s: 11.2x real time, 1867.5 ns per stereo sample` |
| 1x | Faust, faust-rs -lang wasm, repeat, under node | 19.3x | `Faust wasm (node v22.12.0): 5.000 s of stereo audio at 48000 Hz in 0.2594 s: 19.3x real time` |
| 1x | Faust, faustprobe (Cranelift JIT), repeat | 25.8x | `time: compute 194 ms for 5.00 s of audio (240000 frames in 3750 blocks): 25.8x real time` |
| 1x | Faust, faustprobe (Cranelift JIT), unrolled | 9.2x | `time: compute 540 ms for 5.00 s of audio (240000 frames in 3750 blocks): 9.25x real time` |
| 2x | C++ original (SPModel.h) | 26.4x | `C++ original: 5.000 s of stereo audio at 48000 Hz x2 in 0.1893 s: 26.4x real time, 788.8 ns per stereo sample` |
| 2x | Faust, faust-rs -lang cpp, repeat | 27.1x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.1842 s: 27.1x real time, 767.4 ns per stereo sample` |
| 2x | Faust, faust-rs -lang cpp, unrolled (12) | 5.7x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.8724 s: 5.7x real time, 3635.0 ns per stereo sample` |
| 2x | Faust, faust (C++) -lang cpp, unrolled (12) | 6.1x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.8185 s: 6.1x real time, 3410.6 ns per stereo sample` |
| 2x | Faust, faust-rs -lang wasm, repeat, under node | 10.2x | `Faust wasm (node v22.12.0): 5.000 s of stereo audio at 48000 Hz in 0.4892 s: 10.2x real time` |
| 2x | Faust, faustprobe (Cranelift JIT), repeat | 13.5x | `time: compute 371 ms for 5.00 s of audio (240000 frames in 3750 blocks): 13.5x real time` |
| 2x | Faust, faustprobe (Cranelift JIT), unrolled | 4.7x | `time: compute 1.06 s for 5.00 s of audio (240000 frames in 3750 blocks): 4.73x real time` |
| 4x | C++ original (SPModel.h) | 9.2x | `C++ original: 5.000 s of stereo audio at 48000 Hz x4 in 0.5421 s: 9.2x real time, 2258.8 ns per stereo sample` |
| 4x | Faust, faust-rs -lang cpp, repeat | 10.4x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 0.4793 s: 10.4x real time, 1997.0 ns per stereo sample` |
| 4x | Faust, faust-rs -lang cpp, unrolled (12) | 2.9x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 1.7492 s: 2.9x real time, 7288.3 ns per stereo sample` |
| 4x | Faust, faust (C++) -lang cpp, unrolled (12) | 3.0x | `Faust (C++ backend): 5.000 s of stereo audio at 48000 Hz in 1.6694 s: 3.0x real time, 6956.0 ns per stereo sample` |
| 4x | Faust, faust-rs -lang wasm, repeat, under node | 4.4x | `Faust wasm (node v22.12.0): 5.000 s of stereo audio at 48000 Hz in 1.1415 s: 4.4x real time` |
| 4x | Faust, faustprobe (Cranelift JIT), repeat | 5.4x | `time: compute 929 ms for 5.00 s of audio (240000 frames in 3750 blocks): 5.38x real time` |
| 4x | Faust, faustprobe (Cranelift JIT), unrolled | 2.4x | `time: compute 2.09 s for 5.00 s of audio (240000 frames in 3750 blocks): 2.39x real time` |
