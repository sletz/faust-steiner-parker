# The Steiner-Parker filter of Sofia Vallejo in Faust, checked against the original C++.
#
#   make help        this text (also what a bare `make` prints)
#   make test        every check of scripts/test_sp.py against the original C++ (ref/SPModel.h):
#                    the circuit alone, the plugin chain at 1x/2x/4x, the 8 factory presets, the
#                    stress settings, both forms of the Newton loop, the C++ backends, the wasm,
#                    and the CPU cost (best of 3); writes results/results.md and results/results.json and
#                    fails when a check fails (about fifteen minutes)
#   make quick       the same on shorter renders, without the CPU timings (about two minutes)
#   make check       compile every program of dsp/ with faustprobe (one frame each), and the
#                    unrolled ones with the C++ Faust compiler too
#   make ref         build/sp_ref, the renderer of the original model
#   make cost        the CPU cost alone: the original C++ against the Faust programs (best of 3)
#   make web         the web version: dsp/steinerparker_rs*.dsp compiled to wasm by faust-rs
#                    into web/dsp (make -B web after a faust-rs change)
#   make web-check   the wasm DSPs against faustprobe, and their cost, under node
#   make web-serve [PORT=8765]
#                    serve the repository; open http://localhost:PORT/web/
#   make juce [QUALITY=2]
#                    the JUCE plugin (AU, VST3, Standalone) by faust2juce, with the faceplate of
#                    the original (juce/SteinerParkerEditor.h) and its presets (juce/presets.json),
#                    in build/juce/steinerparker-QUALITYx; CMake downloads JUCE 8.0.10 the first time
#   make clean       remove build/
#
# FAUST_RS points to a faust-rs checkout with release faustprobe and faust-rs built
# (cargo build --release -p cranelift-ffi --bin faustprobe; cargo build --release -p compiler
# --bin faust-rs); FAUST is the C++ Faust compiler (for faust2juce and the unrolled form).

FAUST_RS ?= ../faust-rs
FAUSTPROBE ?= $(FAUST_RS)/target/release/faustprobe
FAUSTRS ?= $(FAUST_RS)/target/release/faust-rs
FAUST ?= faust
CXX ?= c++
CXXFLAGS = -std=c++17 -O3 -ffp-contract=off
PORT ?= 8765
QUALITY ?= 2
JUCE_DIR ?=

export FAUST_RS FAUSTPROBE FAUSTRS FAUST CXX

LIB = dsp/steinerparker.lib dsp/repeat.lib
WEB_DSP = web/dsp
JUCE_SRC = $(if $(filter 2,$(QUALITY)),dsp/steinerparker.dsp,dsp/steinerparker_$(QUALITY)x.dsp)
JUCE_OUT = build/juce/steinerparker-$(QUALITY)x

.PHONY: help test quick check ref cost web web-check web-serve juce clean
.DEFAULT_GOAL := help

help:
	@awk 'NR > 1 && !/^#/ { exit } { sub(/^# ?/, ""); print }' $(firstword $(MAKEFILE_LIST))

ref: build/sp_ref

build/sp_ref: ref/sp_ref.cpp ref/SPModel.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ ref/sp_ref.cpp

test: web
	python3 scripts/test_sp.py

quick: web
	python3 scripts/test_sp.py --quick --no-time

check:
	@for f in dsp/*.dsp; do printf "%-36s faustprobe " $$f; $(FAUSTPROBE) --double -I dsp -n 1 --in zero --quiet $$f >/dev/null && echo ok || echo FAIL; done
	@for f in dsp/sp_core.dsp dsp/steinerparker.dsp dsp/steinerparker_1x.dsp dsp/steinerparker_4x.dsp; do printf "%-36s faust      " $$f; $(FAUST) -double -I dsp $$f -o /dev/null && echo ok || echo FAIL; done

cost: build/sp_ref web
	python3 scripts/test_sp.py --time-only

web: $(WEB_DSP)/sp1x.wasm $(WEB_DSP)/sp2x.wasm $(WEB_DSP)/sp4x.wasm

$(WEB_DSP)/sp2x.wasm: dsp/steinerparker_rs.dsp $(LIB)
	@mkdir -p $(WEB_DSP)
	$(FAUSTRS) -lang wasm -double -I dsp $< -o $@

$(WEB_DSP)/sp%x.wasm: dsp/steinerparker_rs_%x.dsp $(LIB)
	@mkdir -p $(WEB_DSP)
	$(FAUSTRS) -lang wasm -double -I dsp $< -o $@

web-check: web
	@for q in 1 2 4; do node scripts/web_check.mjs time $$q 5; done

web-serve:
	python3 -m http.server $(PORT) --bind 127.0.0.1

juce:
	@mkdir -p $(JUCE_OUT)
	rm -rf $(JUCE_OUT)/steinerparker* dsp/steinerparker dsp/steinerparker_$(QUALITY)x
	faust2juce -double -presets juce/presets.json -I $(CURDIR)/dsp $(JUCE_SRC)
	mv $(basename $(JUCE_SRC)) $(JUCE_OUT)/steinerparker
	cmake -S $(JUCE_OUT)/steinerparker -B $(JUCE_OUT)/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
	  -DFAUST_CUSTOM_EDITOR=$(CURDIR)/juce/SteinerParkerEditor.h \
	  "-DCMAKE_CXX_FLAGS=-DSP_QUALITY=$(QUALITY) -ffp-contract=off" $(if $(JUCE_DIR),-DJUCE_DIR=$(JUCE_DIR))
	cmake --build $(JUCE_OUT)/build --target steinerparker_Standalone steinerparker_VST3 steinerparker_AU
	@ls -d $(JUCE_OUT)/build/*_artefacts/Release/*

clean:
	rm -rf build
