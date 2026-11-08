// The wasm DSPs of the web version (web/dsp/sp{1,2,4}x.wasm, faust-rs -lang wasm
// -double) under node, through the page's own loader (web/faust-wasm-dsp.js):
//
//   node scripts/web_check.mjs render <quality> <in.f64> <out.f64> [key=value ...]
//       renders the left output, in blocks of 128 frames as the AudioWorklet does;
//   node scripts/web_check.mjs time <quality> <seconds>
//       the default settings on a stereo 55 Hz saw at 48 kHz, timed.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { FaustWasmDsp } from "../web/faust-wasm-dsp.js";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const [mode, quality, ...rest] = process.argv.slice(2);
const name = join(root, "web", "dsp", `sp${quality}x`);
const compiled = {
  module: new WebAssembly.Module(readFileSync(name + ".wasm")),
  json: JSON.parse(readFileSync(name + ".json", "utf8")),
};
const fs = 48000, Q = 128;
const dsp = new FaustWasmDsp(compiled, fs, Q);

if (mode === "render") {
  const [inPath, outPath, ...sets] = rest;
  for (const s of sets) { const [k, v] = s.split("="); dsp.set(k, Number(v)); }
  const b = readFileSync(inPath);
  const x = new Float64Array(b.buffer, b.byteOffset, b.byteLength / 8);
  const y = new Float64Array(x.length);
  for (let p = 0; p < x.length; p += Q) {
    const n = Math.min(Q, x.length - p);
    for (let c = 0; c < 2; c += 1) dsp.input(c).set(x.subarray(p, p + n));
    dsp.compute(n);
    y.set(dsp.output(0).subarray(0, n), p);
  }
  writeFileSync(outPath, Buffer.from(y.buffer));
} else if (mode === "time") {
  const seconds = Number(rest[0] || 5), total = Math.round(seconds * fs);
  let busy = 0, sink = 0;
  for (let p = 0; p < total; p += Q) {
    const n = Math.min(Q, total - p);
    const l = dsp.input(0), r = dsp.input(1);
    for (let i = 0; i < n; i += 1) { const ph = ((p + i) * 55 / fs) % 1, v = 0.9 * (2 * ph - 1); l[i] = v; r[i] = -v; }
    const t0 = performance.now();
    dsp.compute(n);
    busy += performance.now() - t0;
    sink += dsp.output(0)[0];
  }
  if (sink === 12345) console.log(" ");
  console.log(`Faust wasm (node ${process.version}): ${seconds.toFixed(3)} s of stereo audio at ${fs} Hz in ${(busy / 1000).toFixed(4)} s: ${(seconds / (busy / 1000)).toFixed(1)}x real time`);
} else {
  console.error("usage: see the header of scripts/web_check.mjs");
  process.exit(1);
}
