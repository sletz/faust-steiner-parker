declare name "sp_core";
declare version "0.1.0";
declare author "faust-steiner-parker";
declare license "AGPL-3.0";
// The circuit alone at the rate SR, as `model_test` of the original drives it:
// the input is the source voltage after IC1A (volts), the output Vo1 (volts),
// one tick per sample, the state from the DC point. res is the RES pot (0..1).
// Compared with `build/sp_ref core` by scripts/test_sp.py.
sp = library("steinerparker.lib");
jack = nentry("jack", 0, 0, 2, 1);
cut = hslider("cut", 0.6, 0, 1, 0.001);
cv = hslider("cv", 0, -3, 3, 0.01);
res = hslider("res", 0.6, 0, 1, 0.001);
t1 = hslider("t1", 0.5, 0, 1, 0.001);
N = 8;
first = 1 - 1';
ctl = sp.controls(cut, cv, res, t1);
init = (sp.coefs3(0, jack), ctl) : sp.dc_state(40);
process = _ <: (sp.coefs3(ma.SR, jack), ctl, first, init, _) : sp.voice(1, N)
with { ma = library("maths.lib"); };
