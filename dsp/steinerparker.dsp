declare name "Steiner-Parker Filter";
declare description "Nonlinear wave digital model of the Yusynth Steiner-Parker VCF (Vult build), after Sofia Vallejo's plugin; quality 2x";
declare version "0.1.0";
declare author "GRAME";
declare license "AGPL-3.0";
declare options "[double]";
sp = library("steinerparker.lib");
// oversampling (the Quality selector of the plugin), Newton iterations per
// sub-sample, Newton iterations for the DC point: 12 and 40 are the budgets
// that reproduce the original at every setting measured (README, section 3)
process = sp.plugin(2, 12, 40);
