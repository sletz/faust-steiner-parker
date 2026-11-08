// The page: the faceplate of the original plugin drawn in SVG, its presets, and a
// test bench (a source, a cutoff sweep, a CPU meter) around the AudioWorklet of
// sp-processor.js.
//
// The faceplate follows plugin/Source/PluginEditor.{h,cpp} of the original
// (Copyright (C) 2026 Sofia Vallejo, AGPL-3.0-only): a 460 x 780 faceplate under a
// 48-pixel preset strip, the same colours, knob radii, scales and rotation angles
// (JUCE's: 0 at twelve o'clock, clockwise; a knob turns from 1.25 pi to 2.75 pi, a
// selector from 1.75 pi to 2.25 pi). A knob is dragged up and down (220 pixels for
// its whole range, 120 for a selector), turned with the wheel or the arrow keys, and
// reset by a double click; a selector's labels are clickable.

const DSP_DIR = "dsp/";
const NS = "http://www.w3.org/2000/svg";
const W = 460, FACE = 780;
const C = {
  panel: "#1e3a4c", panelHi: "#26475c", silk: "#f2ead3", rail: "#b9bcc0",
  core: "#e0533d", mod: "#f2b134", signal: "#4fb3a9", select: "#f2ead3",
};

// The controls of dsp/steinerparker.lib, by the key the wasm DSP knows them by.
const zeroToTen = [0, 2, 4, 6, 8, 10].map((v) => [v / 10, String(v)]);
const KNOBS = [
  { key: "Filter/Frequency", title: "FREQUENCY", cap: C.core, r: 46, ticks: 21, marks: zeroToTen, x: 30, y: 104,
    min: 0, max: 1, init: 0.51, fmt: (v) => v.toFixed(3) },
  { key: "Filter/Resonance", title: "RESONANCE", cap: C.core, r: 46, ticks: 21, marks: zeroToTen, x: 230, y: 104,
    min: 0, max: 1, init: 0.4, fmt: (v) => v.toFixed(3) },
  { key: "Modulation/CV_offset", title: "CV OFFSET", cap: C.mod, r: 30, ticks: 13, marks: [[0, "-3"], [0.5, "0"], [1, "+3"]],
    x: 30, y: 300, min: -3, max: 3, init: 0, fmt: (v) => `${v.toFixed(2)} V` },
  { key: "Modulation/V-oct_trim", title: "V/OCT TRIM", cap: C.mod, r: 30, ticks: 11, marks: [[0, "0"], [0.5, "5"], [1, "10"]],
    x: 230, y: 300, min: 0, max: 1, init: 0.45, fmt: (v) => v.toFixed(3) },
  { key: "Signal/Input_level", title: "INPUT LEVEL", cap: C.signal, r: 34, ticks: 13, marks: [[0, "-24"], [0.5, "0"], [1, "+24"]],
    x: 30, y: 468, min: -24, max: 24, init: 0, fmt: (v) => `${v.toFixed(1)} dB` },
  { key: "Signal/Response", title: "RESPONSE", cap: C.select, r: 30, ticks: 3, marks: [[0, "LP"], [0.5, "BP"], [1, "HP"]],
    x: 230, y: 468, min: 0, max: 2, init: 0, selector: true },
  { key: "Settings/Output", title: "OUTPUT", cap: C.signal, r: 24, ticks: 9, marks: [[0, "-36"], [0.75, "0"], [1, "+12"]],
    x: 30, y: 642, min: -36, max: 12, init: -6, fmt: (v) => `${v.toFixed(1)} dB` },
  // Not a control of the DSP: the three compiled programs (1x, 2x, 4x oversampling).
  { key: "quality", title: "QUALITY", cap: C.select, r: 24, ticks: 3, marks: [[0, "1x"], [0.5, "2x"], [1, "4x"]],
    x: 230, y: 642, min: 0, max: 2, init: 1, selector: true },
];

// The factory presets of the original (plugin/Source/PresetManager.h):
// name, response, cutoff, resonance, cv, drive, output, quality index, trim.
const PRESETS = [
  ["Init", 0, 0.51, 0.40, 0.0, 0.0, -6.0, 1, 0.45],
  ["Warm Low-pass", 0, 0.46, 0.30, 0.0, 3.0, -6.0, 1, 0.45],
  ["Acid Squelch", 0, 0.49, 0.85, 0.0, 9.0, -9.0, 1, 0.45],
  ["Vocal Band", 1, 0.53, 0.70, 0.0, 0.0, -3.0, 1, 0.45],
  ["Thin High-pass", 2, 0.55, 0.30, 0.0, 0.0, -6.0, 1, 0.45],
  ["Bright Air", 2, 0.69, 0.55, 0.0, -6.0, -3.0, 1, 0.45],
  ["Dark Drone", 0, 0.32, 0.60, 0.0, 12.0, -6.0, 1, 0.45],
  ["Self-oscillation", 0, 0.44, 0.97, 0.0, 0.0, -12.0, 2, 0.45],
];

const $ = (id) => document.getElementById(id);
const el = (tag, attrs = {}, parent = null) => {
  const e = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (parent) parent.appendChild(e);
  return e;
};
// JUCE's Point::getPointOnCircumference: angle 0 at twelve o'clock, clockwise.
const onCircle = (cx, cy, r, a) => [cx + r * Math.sin(a), cy - r * Math.cos(a)];

const values = new Map(KNOBS.map((k) => [k.key, k.init]));
const views = new Map();
const audio = { ctx: null, node: null, source: null, sourceKind: null, programs: null, starting: false };

// ------------------------------------------------------------------ faceplate
function buildFace() {
  const svg = el("svg", { viewBox: `0 0 ${W} ${FACE}`, role: "group", "aria-label": "Steiner-Parker faceplate" });
  const defs = el("defs", {}, svg);
  const g0 = el("linearGradient", { id: "faceGrad", x1: 0, y1: 0, x2: 0, y2: 1 }, defs);
  el("stop", { offset: 0, "stop-color": C.panelHi }, g0);
  el("stop", { offset: 1, "stop-color": C.panel }, g0);
  el("rect", { x: 0, y: 0, width: W, height: FACE, fill: "url(#faceGrad)" }, svg);
  // mounting rails and screws
  for (const y of [0, FACE - 16]) {
    el("rect", { x: 0, y, width: W, height: 16, fill: C.rail, "fill-opacity": 0.18 }, svg);
    for (const x of [22, W - 22]) {
      el("circle", { cx: x, cy: y + 8, r: 6, fill: "#c9ccd0" }, svg);
      el("line", { x1: x - 3.5, y1: y + 8, x2: x + 3.5, y2: y + 8, stroke: "#6c7278", "stroke-width": 1.5 }, svg);
      el("line", { x1: x, y1: y + 4.5, x2: x, y2: y + 11.5, stroke: "#6c7278", "stroke-width": 1.5 }, svg);
    }
  }
  text(svg, W / 2, 39, "STEINER-PARKER", { size: 28, weight: "bold" });
  text(svg, W / 2, 64, "WAVE DIGITAL MODEL\u00a0\u00a0 / \u00a0\u00a0AFTER YUSYNTH\u00a0\u00a0 / \u00a0\u00a0FAUST", { size: 11.5, opacity: 0.6 });
  // section headers: colour key, name, then a hairline after the name (its width
  // measured once the SVG is in the document, in finishFace)
  for (const [y, c, name] of [[92, C.core, "FILTER"], [288, C.mod, "MODULATION"], [456, C.signal, "SIGNAL"], [630, C.select, "SETTINGS"]]) {
    el("rect", { x: 30, y: y - 1.5, width: 22, height: 3, rx: 1.5, fill: c }, svg);
    const t = text(svg, 60, y, name, { size: 10, weight: "bold", opacity: 0.55, anchor: "start" });
    hairlines.push([t, el("rect", { y, height: 1, fill: C.silk, "fill-opacity": 0.16 }, svg)]);
  }
  const footer = text(svg, W / 2, FACE - 8, "SOFIA VALLEJO\u00a0 / \u00a0IEM GRAZ\u00a0\u00a0 - \u00a0\u00a0FAUST PORT", { size: 9, weight: "bold", opacity: 0.8 });
  footer.setAttribute("class", "footer");
  for (const k of KNOBS) buildKnob(svg, k);
  const bubble = el("g", { class: "bubble", visibility: "hidden" }, svg);
  el("rect", { rx: 3, height: 20, width: 80 }, bubble);
  el("text", { "text-anchor": "middle", "dominant-baseline": "central" }, bubble);
  views.set("bubble", bubble);
  $("face").appendChild(svg);
  for (const [t, line] of hairlines) {
    const x = 60 + t.getComputedTextLength() + 12;
    line.setAttribute("x", x);
    line.setAttribute("width", W - 30 - x);
  }
}
const hairlines = [];

function text(parent, x, y, s, { size = 11, weight = "normal", opacity = 1, anchor = "middle", fill = C.silk } = {}) {
  const t = el("text", {
    x, y, "font-size": size, "font-weight": weight, fill, "fill-opacity": opacity,
    "text-anchor": anchor, "dominant-baseline": "central",
  }, parent);
  t.textContent = s;
  return t;
}

function buildKnob(svg, k) {
  const start = k.selector ? Math.PI * 1.75 : Math.PI * 1.25;
  const end = k.selector ? Math.PI * 2.25 : Math.PI * 2.75;
  const cx = k.x + 100, cy = k.y + 22 + k.r + 26;   // PanelKnob::centre()
  const g = el("g", { class: "knob", tabindex: 0, role: "slider", "aria-label": k.title }, svg);
  text(g, cx, k.y + 9, k.title, { size: 13.5, weight: "bold" });
  for (let i = 0; i < k.ticks; i += 1) {
    const pos = k.ticks > 1 ? i / (k.ticks - 1) : 0.5;
    const a = start + pos * (end - start);
    const major = k.selector || i % 2 === 0;
    const [x0, y0] = onCircle(cx, cy, k.r + 5, a);
    const [x1, y1] = onCircle(cx, cy, k.r + (major ? 12 : 9), a);
    el("line", { x1: x0, y1: y0, x2: x1, y2: y1, stroke: C.silk, "stroke-opacity": 0.85, "stroke-width": major ? 1.6 : 1.1 }, g);
  }
  const labels = k.marks.map(([pos, s], i) => {
    const [x, y] = onCircle(cx, cy, k.r + (k.selector ? 22 : 24), start + pos * (end - start));
    const t = text(g, x, y, s, { size: k.selector ? 13 : 11, opacity: 0.8 });
    if (k.selector) {
      t.setAttribute("class", "choice");
      t.addEventListener("pointerdown", (e) => { e.stopPropagation(); setValue(k, k.min + i); });
    }
    return t;
  });
  // drawRotarySlider: skirt of radius r with 28 teeth, cap of 0.74 r, pointer
  const r = k.r, cr = r * 0.74;
  el("circle", { cx: cx + 1.5, cy: cy + 3, r, fill: "#000", "fill-opacity": 0.35 }, g);
  el("circle", { cx, cy, r, fill: "#15191d", class: "skirt" }, g);
  for (let i = 0; i < 28; i += 1) {
    const [x, y] = onCircle(cx, cy, r - 1.5, (2 * Math.PI * i) / 28);
    el("circle", { cx: x, cy: y, r: 1.4, fill: "#2c3338" }, g);
  }
  const id = `cap-${k.key.replace(/[^a-z]/gi, "")}`;
  const grad = el("radialGradient", { id, cx: 0.2, cy: 0.15, r: 1.0 }, svg.querySelector("defs"));
  el("stop", { offset: 0, "stop-color": shade(k.cap, 0.35) }, grad);
  el("stop", { offset: 1, "stop-color": shade(k.cap, -0.35) }, grad);
  el("circle", { cx, cy, r: cr, fill: `url(#${id})`, stroke: "#000", "stroke-opacity": 0.25 }, g);
  const light = k.cap === C.select;
  const pointer = el("line", { stroke: light ? "#1b1f23" : C.silk, "stroke-width": Math.max(2, r * 0.08), "stroke-linecap": "butt" }, g);
  views.set(k.key, { k, g, pointer, labels, cx, cy, start, end });
  attachKnob(k, g);
  draw(k);
}

// A colour made brighter (amount > 0) or darker (< 0), as JUCE's brighter()/darker() roughly do.
function shade(hex, amount) {
  const n = parseInt(hex.slice(1), 16);
  const ch = [(n >> 16) & 255, (n >> 8) & 255, n & 255].map((v) =>
    Math.round(amount > 0 ? v + (255 - v) * amount * 0.6 : v * (1 + amount)));
  return `rgb(${ch.join(",")})`;
}

function draw(k) {
  const v = views.get(k.key);
  const value = values.get(k.key);
  const pos = (value - k.min) / (k.max - k.min);
  const a = v.start + pos * (v.end - v.start);
  const [x0, y0] = onCircle(v.cx, v.cy, k.r * 0.74 * 0.25, a);
  const [x1, y1] = onCircle(v.cx, v.cy, k.r - 2, a);
  v.pointer.setAttribute("x1", x0); v.pointer.setAttribute("y1", y0);
  v.pointer.setAttribute("x2", x1); v.pointer.setAttribute("y2", y1);
  v.g.setAttribute("aria-valuenow", value);
  if (k.selector) {
    const sel = Math.round(value - k.min);
    v.labels.forEach((t, i) => {
      t.setAttribute("fill", i === sel ? C.mod : C.silk);
      t.setAttribute("fill-opacity", i === sel ? 1 : 0.75);
      t.setAttribute("font-weight", i === sel ? "bold" : "normal");
    });
    v.g.setAttribute("aria-valuetext", k.marks[sel][1]);
  } else {
    v.g.setAttribute("aria-valuetext", k.fmt(value));
  }
}

function attachKnob(k, g) {
  const range = k.max - k.min;
  const pixels = k.selector ? 120 : 220;
  let drag = null;
  g.addEventListener("pointerdown", (e) => {
    drag = { y: e.clientY, v: values.get(k.key), scale: g.ownerSVGElement.getBoundingClientRect().width / W };
    g.setPointerCapture(e.pointerId);
    showBubble(k);
    e.preventDefault();
  });
  g.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const dy = (drag.y - e.clientY) / drag.scale;
    setValue(k, drag.v + (dy / pixels) * range, true);
    showBubble(k);
  });
  const stop = () => { drag = null; hideBubble(); };
  g.addEventListener("pointerup", stop);
  g.addEventListener("pointercancel", stop);
  g.addEventListener("dblclick", () => { if (!k.selector) setValue(k, k.init); });
  g.addEventListener("wheel", (e) => {
    e.preventDefault();
    const step = k.selector ? 1 : range / 200;
    setValue(k, values.get(k.key) + (e.deltaY < 0 ? step : -step));
  }, { passive: false });
  g.addEventListener("keydown", (e) => {
    const step = k.selector ? 1 : range / (e.shiftKey ? 1000 : 100);
    if (e.key === "ArrowUp" || e.key === "ArrowRight") setValue(k, values.get(k.key) + step);
    else if (e.key === "ArrowDown" || e.key === "ArrowLeft") setValue(k, values.get(k.key) - step);
    else return;
    e.preventDefault();
  });
}

function showBubble(k) {
  if (k.selector) return;
  const v = views.get(k.key), b = views.get("bubble");
  const s = k.fmt(values.get(k.key));
  const w = 12 + s.length * 7.5;
  b.querySelector("rect").setAttribute("x", v.cx - w / 2);
  b.querySelector("rect").setAttribute("y", v.cy - k.r - 34);
  b.querySelector("rect").setAttribute("width", w);
  const t = b.querySelector("text");
  t.setAttribute("x", v.cx); t.setAttribute("y", v.cy - k.r - 24);
  t.textContent = s;
  b.setAttribute("visibility", "visible");
}
function hideBubble() { views.get("bubble").setAttribute("visibility", "hidden"); }

function setValue(k, value, fromDrag = false) {
  let v = Math.min(k.max, Math.max(k.min, value));
  if (k.selector) v = Math.round(v);
  if (values.get(k.key) === v) return;
  values.set(k.key, v);
  draw(k);
  if (!fromDrag && !k.selector) { /* keep the preset label until a value is changed by hand */ }
  send(k.key, v);
  if (!syncingPreset) markEdited();
}

function send(key, value) {
  if (!audio.node) return;
  if (key === "quality") audio.node.port.postMessage({ type: "quality", value: [1, 2, 4][value] });
  else audio.node.port.postMessage({ type: "control", key, value });
}

// ------------------------------------------------------------------ presets
let syncingPreset = false;
function buildPresets() {
  const sel = $("preset");
  PRESETS.forEach((p, i) => sel.add(new Option(p[0], String(i))));
  sel.add(new Option("(edited)", "edited"));
  sel.lastChild.hidden = true;
  sel.addEventListener("change", () => { if (sel.value !== "edited") loadPreset(Number(sel.value)); });
  $("prev").addEventListener("click", () => step(-1));
  $("next").addEventListener("click", () => step(1));
}
function step(d) {
  const sel = $("preset");
  const i = sel.value === "edited" ? 0 : Number(sel.value);
  loadPreset((i + d + PRESETS.length) % PRESETS.length);
}
function loadPreset(i) {
  const [, mode, cut, res, cv, drive, out, q, t1] = PRESETS[i];
  const byKey = { "Signal/Response": mode, "Filter/Frequency": cut, "Filter/Resonance": res, "Modulation/CV_offset": cv,
    "Signal/Input_level": drive, "Settings/Output": out, quality: q, "Modulation/V-oct_trim": t1 };
  syncingPreset = true;
  for (const k of KNOBS) setValue(k, byKey[k.key]);
  syncingPreset = false;
  $("preset").value = String(i);
}
function markEdited() { $("preset").value = "edited"; }

// ------------------------------------------------------------------ audio
async function loadProgram(name) {
  const [json, module] = await Promise.all([
    fetch(DSP_DIR + name + ".json").then((r) => r.json()),
    fetch(DSP_DIR + name + ".wasm").then(async (r) => WebAssembly.compile(await r.arrayBuffer())),
  ]);
  return { json, module };
}

async function start() {
  if (audio.starting) return;
  audio.starting = true;
  try {
    const ctx = new AudioContext({ latencyHint: "playback" });
    await ctx.audioWorklet.addModule("sp-processor.js");
    if (!audio.programs) {
      const [p1, p2, p4] = await Promise.all(["sp1x", "sp2x", "sp4x"].map(loadProgram));
      audio.programs = { 1: p1, 2: p2, 4: p4 };
    }
    const controls = {};
    for (const k of KNOBS) if (k.key !== "quality") controls[k.key] = values.get(k.key);
    const node = new AudioWorkletNode(ctx, "steiner-parker", {
      numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2],
      channelCount: 2, channelCountMode: "explicit", channelInterpretation: "speakers",
      processorOptions: { programs: audio.programs, quality: [1, 2, 4][values.get("quality")], controls },
    });
    node.port.onmessage = (e) => {
      if (e.data.type === "stats") $("load").textContent = `${(100 * e.data.load).toFixed(1)} % (${e.data.quality}x, ${ctx.sampleRate} Hz)`;
    };
    node.connect(ctx.destination);
    audio.ctx = ctx;
    audio.node = node;
    await setSource($("source").value);
    $("power").textContent = "Stop audio";
    $("power").classList.add("on");
  } catch (err) {
    $("load").textContent = `error: ${err.message}`;
    console.error(err);
  } finally {
    audio.starting = false;
  }
}

async function stop() {
  if (!audio.ctx) return;
  await audio.ctx.close();
  Object.assign(audio, { ctx: null, node: null, source: null, sourceKind: null });
  $("power").textContent = "Start audio";
  $("power").classList.remove("on");
  $("load").textContent = "–";
}

async function setSource(kind) {
  $("pitchBox").hidden = kind !== "saw";
  if (!audio.ctx) return;
  const ctx = audio.ctx;
  if (audio.source) {
    try { audio.source.stop?.(); } catch (e) { /* already stopped */ }
    audio.source.disconnect();
    audio.source.mediaStream?.getTracks().forEach((t) => t.stop());
    audio.source = null;
  }
  let src;
  if (kind === "saw") {
    // A band-limited saw of amplitude 1.0 (5 V at the jack), the article's test signal.
    src = new OscillatorNode(ctx, { type: "sawtooth", frequency: Number($("pitch").value) });
    src.start();
  } else if (kind === "noise") {
    const buf = new AudioBuffer({ length: ctx.sampleRate * 2, numberOfChannels: 1, sampleRate: ctx.sampleRate });
    const d = buf.getChannelData(0);
    for (let i = 0; i < d.length; i += 1) d[i] = (Math.random() * 2 - 1) * 0.5;
    src = new AudioBufferSourceNode(ctx, { buffer: buf, loop: true });
    src.start();
  } else if (kind === "file") {
    const f = $("file").files[0];
    if (!f) { $("file").click(); return; }
    const buf = await ctx.decodeAudioData(await f.arrayBuffer());
    src = new AudioBufferSourceNode(ctx, { buffer: buf, loop: true });
    src.start();
  } else if (kind === "mic") {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    src = new MediaStreamAudioSourceNode(ctx, { mediaStream: stream });
  }
  src.connect(audio.node);
  audio.source = src;
  audio.sourceKind = kind;
}

// The article's listening examples: a slow cutoff sweep up and down.
let sweepStart = null;
function sweepTick(t) {
  if (!$("sweep").checked) { sweepStart = null; return; }
  if (sweepStart === null) sweepStart = t;
  const phase = ((t - sweepStart) / 16000) % 1;
  const tri = phase < 0.5 ? phase * 2 : 2 - phase * 2;
  setValue(KNOBS[0], 0.25 + 0.5 * tri, true);
  requestAnimationFrame(sweepTick);
}

// ------------------------------------------------------------------ wiring
buildFace();
buildPresets();
loadPreset(0);
$("power").addEventListener("click", () => (audio.ctx ? stop() : start()));
$("source").addEventListener("change", () => setSource($("source").value).catch((e) => { $("load").textContent = `error: ${e.message}`; }));
$("file").addEventListener("change", () => setSource("file").catch((e) => { $("load").textContent = `error: ${e.message}`; }));
$("pitch").addEventListener("input", () => {
  $("pitchOut").textContent = `${Number($("pitch").value)} Hz`;
  if (audio.sourceKind === "saw") audio.source.frequency.value = Number($("pitch").value);
});
$("sweep").addEventListener("change", () => { if ($("sweep").checked) requestAnimationFrame(sweepTick); });
