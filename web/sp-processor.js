// The Steiner-Parker filter in an AudioWorklet: one faust-rs wasm DSP
// (dsp/steinerparker_rs*.dsp, compiled with -double by `make web`), stereo in,
// stereo out, the page's controls applied to it.
//
// The three Quality settings are three compiled programs (the oversampling of a
// Faust program is fixed when it is compiled). Changing the Quality makes a new
// instance of the chosen program with the current controls: its state starts at
// the DC operating point, as the original plugin's reconfigure() does.
//
// A DSP whose output is not finite is replaced by a fresh instance, as the
// original clears its buffer and restarts from DC (the Faust program already
// resets its own state when the state stops being finite: this is a second guard).
//
// Messages from the page (port.postMessage):
//   { type: "control", key, value }   a control key ("Filter/Frequency")
//   { type: "quality", value }        1, 2 or 4
// Messages to the page:
//   { type: "ready" }
//   { type: "stats", load, quality }  every half second: the time spent in
//                                     process() over the audio time of the window

import { FaustWasmDsp } from "./faust-wasm-dsp.js";

const QUANTUM = 128;
const STATS_SECONDS = 0.5;

class SteinerParkerProcessor extends AudioWorkletProcessor {
  /**
   * @param {Object} options.processorOptions - { programs: {1, 2, 4: {module, json}},
   *   quality, controls: {key: value} }
   */
  constructor(options) {
    super();
    const { programs, quality, controls } = options.processorOptions;
    this.programs = programs;
    this.controls = new Map(Object.entries(controls || {}));
    this.quality = quality;
    this.dsp = this.make(quality);
    this.busy = 0;
    this.frames = 0;
    this.port.onmessage = (e) => this.message(e.data);
    this.port.postMessage({ type: "ready" });
  }

  make(quality) {
    const dsp = new FaustWasmDsp(this.programs[quality], sampleRate, QUANTUM);
    for (const [k, v] of this.controls) dsp.set(k, v);
    return dsp;
  }

  message(m) {
    if (m.type === "control") {
      this.controls.set(m.key, m.value);
      this.dsp.set(m.key, m.value);
    } else if (m.type === "quality" && m.value !== this.quality && this.programs[m.value]) {
      this.quality = m.value;
      this.dsp = this.make(m.value);
    }
  }

  process(inputs, outputs) {
    const t0 = globalThis.performance ? performance.now() : Date.now();
    const input = inputs[0] || [];
    const output = outputs[0];
    const frames = output[0].length;
    const dsp = this.dsp;
    for (let c = 0; c < 2; c += 1) {
      const src = input[Math.min(c, input.length - 1)];
      const buf = dsp.input(c);
      if (src) for (let i = 0; i < frames; i += 1) buf[i] = src[i];
      else buf.fill(0, 0, frames);
    }
    dsp.compute(frames);
    let finite = true;
    for (let c = 0; c < output.length; c += 1) {
      const out = dsp.output(Math.min(c, 1));
      const dst = output[c];
      for (let i = 0; i < frames; i += 1) {
        const v = out[i];
        if (!Number.isFinite(v)) finite = false;
        dst[i] = v;
      }
    }
    if (!finite) {
      for (const dst of output) dst.fill(0);
      this.dsp = this.make(this.quality);
    }
    const t1 = globalThis.performance ? performance.now() : Date.now();
    this.busy += t1 - t0;
    this.frames += frames;
    if (this.frames >= STATS_SECONDS * sampleRate) {
      this.port.postMessage({ type: "stats", load: this.busy / 1000 / (this.frames / sampleRate), quality: this.quality });
      this.busy = 0;
      this.frames = 0;
    }
    return true;
  }
}

registerProcessor("steiner-parker", SteinerParkerProcessor);
