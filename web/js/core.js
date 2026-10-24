/* SmartCW web interface - core: DOM helpers, device API, live events,
 * Morse table, browser audio, routing and shared widgets. */
"use strict";

window.addEventListener("error", (e) => showFatal(e.message + (e.lineno ? " (line " + e.lineno + ")" : "")));
window.addEventListener("unhandledrejection", (e) => showFatal(String(e.reason && e.reason.message || e.reason)));
function showFatal(msg) {
  let b = document.getElementById("fatal");
  if (!b) {
    b = document.createElement("div");
    b.id = "fatal";
    document.body.appendChild(b);
  }
  b.textContent = "Script error: " + msg;
}

const $ = (s, el = document) => el.querySelector(s);
const $$ = (s, el = document) => Array.from(el.querySelectorAll(s));

// h("div.card#id", {attrs}, children...)
function h(tag, attrs, ...kids) {
  const m = /^([a-z0-9]+)?((?:[.#][\w-]+)*)$/i.exec(tag);
  const el = document.createElement((m && m[1]) || "div");
  if (m && m[2]) for (const part of m[2].match(/[.#][\w-]+/g)) {
    if (part[0] === ".") el.classList.add(part.slice(1)); else el.id = part.slice(1);
  }
  if (attrs && (typeof attrs !== "object" || attrs instanceof Node || Array.isArray(attrs))) {
    kids.unshift(attrs);
    attrs = null;
  }
  for (const k in attrs || {}) {
    const v = attrs[k];
    if (v === undefined || v === null || v === false) continue;
    if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2), v);
    else if (k === "html") el.innerHTML = v;
    else if (k === "text") el.textContent = v;
    else if (k === "style" && typeof v === "object") Object.assign(el.style, v);
    else el.setAttribute(k, v === true ? "" : v);
  }
  const add = (c) => {
    if (c === undefined || c === null || c === false) return;
    if (Array.isArray(c)) c.forEach(add);
    else el.appendChild(c instanceof Node ? c : document.createTextNode(String(c)));
  };
  kids.forEach(add);
  return el;
}

function icon(name, cls) {
  const s = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  s.setAttribute("class", "i" + (cls ? " " + cls : ""));
  const u = document.createElementNS("http://www.w3.org/2000/svg", "use");
  u.setAttribute("href", "#i-" + name);
  s.appendChild(u);
  return s;
}

// ---------------- event bus ----------------
const Bus = {
  m: {},
  on(ev, fn) { (this.m[ev] = this.m[ev] || []).push(fn); return fn; },
  off(ev, fn) { this.m[ev] = (this.m[ev] || []).filter((f) => f !== fn); },
  emit(ev, d) { for (const f of this.m[ev] || []) { try { f(d); } catch (e) { showFatal(e.message); } } },
};

// ---------------- device state and API ----------------
const App = { cfg: {}, rt: {}, meta: [], metaBy: {}, v: "", up: 0, connected: false, txDown: false };

async function getJSON(path) {
  const r = await fetch(path, { cache: "no-store" });
  if (!r.ok) throw new Error(path + ": HTTP " + r.status);
  return r.json();
}

async function post(path, params) {
  const body = new URLSearchParams();
  for (const k in params || {}) body.append(k, params[k]);
  const r = await fetch(path, { method: "POST", body });
  const t = await r.text();
  try { return JSON.parse(t); } catch (e) { return t; }
}

function applyState(s) {
  App.v = s.v;
  App.up = s.up;
  // keep locally pending edits on top of what the device reports
  App.cfg = Object.assign({}, s.cfg, Settings.pending);
  App.rt = s.rt;
  Bus.emit("state", App);
}

// Settings changes are batched and sent 250 ms after the last edit.
const Settings = {
  pending: {},
  timer: null,
  set(k, v) {
    this.pending[k] = v;
    App.cfg[k] = v;
    Bus.emit("cfg", { k, v });
    clearTimeout(this.timer);
    this.timer = setTimeout(() => this.flush(), 250);
  },
  async flush() {
    const p = this.pending;
    this.pending = {};
    if (!Object.keys(p).length) return;
    const params = {};
    for (const k in p) params[k] = typeof p[k] === "boolean" ? (p[k] ? "true" : "false") : p[k];
    try {
      const r = await post("/api/set", params);
      if (r && r.err && Object.keys(r.err).length) {
        for (const k in r.err) toast(r.err[k], "err");
      } else {
        Bus.emit("saved", r);
      }
      if (r && r.restart) App.rt.restart = true;
      refreshState();
    } catch (e) {
      toast("Could not reach SmartCW", "err");
    }
  },
};

async function refreshState() {
  try { applyState(await getJSON("/api/state")); } catch (e) { /* the event stream brings it later */ }
}

// ---------------- live events ----------------
const Live = {
  es: null,
  start() {
    if (this.es) this.es.close();
    const es = new EventSource("/api/events");
    this.es = es;
    es.onopen = () => { App.connected = true; Bus.emit("conn", true); };
    es.onerror = () => { App.connected = false; Bus.emit("conn", false); };
    es.onmessage = (ev) => {
      let m;
      try { m = JSON.parse(ev.data); } catch (e) { return; }
      if (m.e === "state") applyState(m.s);
      else {
        if (m.e === "k") App.txDown = !!(m.d && (m.f & 1));
        Bus.emit(m.e, m);
      }
    };
  },
};

// Trainer pages hold practice mode (no TX) while they are open.
const Practice = {
  holders: 0,
  timer: null,
  hold() {
    this.holders++;
    if (this.holders === 1) {
      const ping = () => post("/api/practice", { ms: 9000 }).catch(() => {});
      ping();
      this.timer = setInterval(ping, 4000);
    }
  },
  release() {
    this.holders = Math.max(0, this.holders - 1);
    if (this.holders === 0) {
      clearInterval(this.timer);
      post("/api/practice", { on: 0 }).catch(() => {});
    }
  },
};

// ---------------- Morse ----------------
const MORSE = {
  A: ".-", B: "-...", C: "-.-.", D: "-..", E: ".", F: "..-.", G: "--.", H: "....", I: "..", J: ".---", K: "-.-",
  L: ".-..", M: "--", N: "-.", O: "---", P: ".--.", Q: "--.-", R: ".-.", S: "...", T: "-", U: "..-", V: "...-",
  W: ".--", X: "-..-", Y: "-.--", Z: "--..", 1: ".----", 2: "..---", 3: "...--", 4: "....-", 5: ".....",
  6: "-....", 7: "--...", 8: "---..", 9: "----.", 0: "-----", ".": ".-.-.-", ",": "--..--", "?": "..--..",
  "=": "-...-", "-": "-....-", "/": "-..-.", "@": ".--.-.", "'": ".----.", '"': ".-..-.", ":": "---...",
  ";": "-.-.-.", "_": "..--.-", ")": "-.--.-", "$": "...-..-", "(": "-.--.", "+": ".-.-.", "&": ".-...",
  "|": "...-.-", "*": "...-.", "^": "-...-.-", "#": "........",
};
const PROSIGN_NAME = { "+": "AR", "(": "KN", "&": "AS", "|": "SK", "*": "SN", "=": "BT", "^": "BK", "#": "HH" };
const NAME_PROSIGN = { AR: "+", KN: "(", AS: "&", SK: "|", VA: "|", SN: "*", VE: "*", BT: "=", BK: "^", HH: "#" };
const DECODE = {};
for (const k in MORSE) DECODE[MORSE[k]] = k;

function decodePattern(p) {
  if (DECODE[p]) return DECODE[p];
  if (p.length >= 6 && /^\.+$/.test(p)) return "#";
  return "";
}
// "<SK>" style prosigns to their stand-in characters, uppercase.
function normalize(text) {
  return String(text).toUpperCase().replace(/<([A-Z]{2})>/g, (m, n) => NAME_PROSIGN[n] || m);
}
// A stand-in character for display.
function disp(c) {
  if (PROSIGN_NAME[c]) return "<" + PROSIGN_NAME[c] + ">";
  return String(c).toUpperCase();
}
function dispText(t) { return Array.from(normalize(t)).map(disp).join(""); }
function patternOf(c) { return MORSE[String(c).toUpperCase()] || ""; }

// Render Morse as gold dots and bars: text -> element
function morseView(text, cls) {
  const box = h("div.morse" + (cls ? "." + cls : ""));
  const words = normalize(text).split(" ");
  words.forEach((w, wi) => {
    if (wi && w.length) box.appendChild(h("span.mw", "/"));
    for (const c of w) {
      const p = MORSE[c];
      if (!p) continue;
      const ch = h("span.mc");
      for (const e of p) ch.appendChild(h(e === "." ? "i.d" : "i.h"));
      box.appendChild(ch);
    }
  });
  return box;
}

// ---------------- browser audio ----------------
const Beep = {
  ctx: null, osc: null, gain: null, enabled: true,
  init() {
    if (this.ctx) return;
    const C = window.AudioContext || window.webkitAudioContext;
    if (!C) return;
    this.ctx = new C();
    this.gain = this.ctx.createGain();
    this.gain.gain.value = 0;
    this.gain.connect(this.ctx.destination);
    this.osc = this.ctx.createOscillator();
    this.osc.type = "sine";
    this.osc.connect(this.gain);
    this.osc.start();
  },
  set(on, freq) {
    if (!this.enabled) on = false;
    if (on) this.init();
    if (!this.ctx) return;
    if (this.ctx.state === "suspended") this.ctx.resume();
    const t = this.ctx.currentTime;
    this.osc.frequency.setValueAtTime(freq || App.cfg.tone || 600, t);
    this.gain.gain.cancelScheduledValues(t);
    this.gain.gain.setTargetAtTime(on ? 0.18 : 0, t, 0.004);
  },
  // Plays text in the browser; returns a promise that resolves when done.
  // eff: Farnsworth effective wpm (0 = off)
  play(text, wpm, freq, eff) {
    this.init();
    if (!this.ctx || !this.enabled) return Promise.resolve();
    wpm = wpm || App.cfg.wpm || 20;
    const dit = 1.2 / wpm;
    let charGap = 3 * dit, wordGap = 7 * dit;
    if (eff && eff < wpm) {
      const ta = (60 * wpm - 37.2 * eff) / (wpm * eff);
      charGap = (3 * ta) / 19;
      wordGap = (7 * ta) / 19;
    }
    let t = this.ctx.currentTime + 0.05;
    const g = this.gain.gain;
    this.osc.frequency.setValueAtTime(freq || App.cfg.tone || 600, t);
    for (const c of normalize(text)) {
      if (c === " ") { t += wordGap - charGap; continue; }
      const p = MORSE[c];
      if (!p) continue;
      for (const e of p) {
        g.setTargetAtTime(0.18, t, 0.004);
        t += (e === "." ? 1 : 3) * dit;
        g.setTargetAtTime(0, t, 0.004);
        t += dit;
      }
      t += charGap - dit;
    }
    return new Promise((res) => setTimeout(res, (t - this.ctx.currentTime) * 1000));
  },
};

// ---------------- toast and banner ----------------
function toast(msg, kind) {
  const box = $("#toast");
  const t = h("div.t" + (kind ? "." + kind : ""), msg);
  box.appendChild(t);
  setTimeout(() => t.classList.add("out"), 2600);
  setTimeout(() => t.remove(), 3100);
}

// ---------------- routing ----------------
const Pages = {};
const Router = {
  cur: null,
  go(hash) { location.hash = hash; },
  route() {
    const hash = location.hash.replace(/^#\/?/, "") || "live";
    const [page, sub] = hash.split("/");
    const p = Pages[page] ? page : "live";
    if (this.cur && this.cur !== p && Pages[this.cur].leave) Pages[this.cur].leave();
    $$("main > section.page").forEach((s) => (s.hidden = s.id !== "p-" + p));
    $$("nav [data-page]").forEach((a) => {
      if (a.dataset.page === p) a.setAttribute("aria-current", "page"); else a.removeAttribute("aria-current");
    });
    const first = this.cur !== p;
    this.cur = p;
    if (Pages[p].show) Pages[p].show(sub, first);
    window.scrollTo(0, 0);
  },
};

// ---------------- widgets ----------------
// Keep a widget in sync with the device state; stops once it left the page.
function bindSync(el, sync) {
  const fn = () => {
    if (el.isConnected) el._seen = true;
    else if (el._seen) { Bus.off("state", fn); Bus.off("cfg", fn); return; }
    sync();
  };
  Bus.on("state", fn);
  Bus.on("cfg", fn);
  sync();
}
// Segmented control bound to an enum setting.
function segmented(key, options, labels) {
  const box = h("div.seg", { role: "group" });
  options.forEach((o, i) => {
    const b = h("button", { type: "button", "data-v": o, onclick: () => Settings.set(key, o) }, labels ? labels[i] : o);
    box.appendChild(b);
  });
  const sync = () => $$("button", box).forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.v === App.cfg[key])));
  bindSync(box, sync);
  return box;
}

// [-] value [+] tile for an integer setting.
// opts: " Hz" (unit) or {unit, fmt(v), zeroBelow} - zeroBelow: values under it snap to 0 ("off")
function stepper(key, label, step, opts) {
  if (typeof opts === "string" || !opts) opts = { unit: opts || "" };
  const val = h("b");
  const meta = () => App.metaBy[key] || { min: 0, max: 9999 };
  const change = (d) => {
    const m = meta();
    let v = Number(App.cfg[key] || 0);
    if (opts.zeroBelow && v === 0 && d > 0) v = opts.zeroStart || opts.zeroBelow;
    else v += d;
    if (opts.zeroBelow && v < opts.zeroBelow) v = 0;
    v = Math.max(m.min, Math.min(m.max, v));
    Settings.set(key, v);
  };
  let rep = null;
  const btn = (txt, d) => h("i", {
    role: "button", "aria-label": (d > 0 ? "more " : "less ") + label,
    onpointerdown: (e) => {
      e.preventDefault();
      change(d);
      clearInterval(rep);
      const start = Date.now();
      rep = setInterval(() => { if (Date.now() - start > 400) change(d); }, 90);
    },
    onpointerup: () => clearInterval(rep), onpointerleave: () => clearInterval(rep), onpointercancel: () => clearInterval(rep),
  }, txt);
  const el = h("div.b.lvl", btn("\u2212", -step), h("span", h("small", label), val), btn("+", step));
  const sync = () => {
    const v = App.cfg[key];
    val.textContent = v === undefined ? "-" : opts.fmt ? opts.fmt(v) : v + (opts.unit || "");
  };
  bindSync(el, sync);
  return el;
}

function switchInput(key) {
  const inp = h("input", { type: "checkbox", onchange: (e) => Settings.set(key, e.target.checked) });
  const sync = () => { inp.checked = !!App.cfg[key]; };
  bindSync(inp, sync);
  return h("label.sw", inp, h("i"));
}

function rangeInput(key, unit, fmt, step) {
  const m = App.metaBy[key] || { min: 0, max: 100 };
  const show = (v) => (fmt ? fmt(Number(v)) : v + (unit || ""));
  const out = h("output");
  const inp = h("input", {
    type: "range", min: m.min, max: m.max, step: step || 1, "aria-label": key,
    oninput: (e) => { out.textContent = show(e.target.value); },
    onchange: (e) => Settings.set(key, Number(e.target.value)),
  });
  const sync = () => {
    if (document.activeElement === inp) return;
    inp.value = App.cfg[key];
    out.textContent = show(App.cfg[key]);
  };
  bindSync(inp, sync);
  return h("div.rng", inp, out);
}

function numInput(key) {
  const m = App.metaBy[key] || {};
  const inp = h("input.num", { type: "number", min: m.min, max: m.max, step: 1, "aria-label": key });
  inp.addEventListener("change", () => Settings.set(key, Number(inp.value)));
  bindSync(inp, () => { if (document.activeElement !== inp) inp.value = App.cfg[key]; });
  return inp;
}

function textInput(key, attrs) {
  const inp = h("input", Object.assign({ type: "text", spellcheck: "false", autocomplete: "off" }, attrs || {}));
  inp.addEventListener("change", () => Settings.set(key, inp.value));
  bindSync(inp, () => { if (document.activeElement !== inp) inp.value = App.cfg[key] || ""; });
  return inp;
}

function card(iconName, title, subtitle, ...body) {
  return h("div.dp-card",
    h("div.dp-head", h("div.dp-ico", icon(iconName)), h("div", h("h3", title), subtitle ? h("p", subtitle) : null)),
    h("div.dp-body", ...body));
}

function row(label, help, ctl, stack) {
  return h("div.dp-row" + (stack ? ".stack" : ""), h("div.dp-lbl", h("b", label), help ? h("span", help) : null), h("div.dp-ctl", ctl));
}

function fmtUptime(s) {
  const d = Math.floor(s / 86400), hh = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return (d ? d + "d " : "") + (d || hh ? hh + "h " : "") + m + "m";
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

function expand(text) {
  return String(text || "").replace(/\{call\}/gi, App.cfg.call || "").replace(/\{name\}/gi, App.cfg.name || "");
}
// For labels: placeholders stay visible while call / name are not set.
function expandShow(text) {
  return String(text || "").replace(/\{call\}/gi, App.cfg.call || "{call}").replace(/\{name\}/gi, App.cfg.name || "{name}");
}
