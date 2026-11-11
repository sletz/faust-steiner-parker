declare name "Steiner-Parker Filter";
declare description "Nonlinear wave digital model of the Yusynth Steiner-Parker VCF (Vult build), after Sofia Vallejo's plugin; quality 4x; faust-rs form, Newton with an exit";
declare version "0.1.0";
declare author "GRAME";
declare license "AGPL-3.0";
declare options "[double]";
sp = library("steinerparker.lib");
// oversampling, Newton loop with an exit (at most 50 iterations, as the
// C++), Newton iterations for the DC point. Needs faust-rs (ondemand).
process = sp.plugin_with(4, sp.repeated(50), 40);
