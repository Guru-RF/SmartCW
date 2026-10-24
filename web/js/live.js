/* Live page: Morse tree, decoded text, on-screen key, modes, sending. */
"use strict";

const LivePrefs = {
  load() {
    let p = {};
    try { p = JSON.parse(localStorage.getItem("smartcw.live") || "{}"); } catch (e) { p = {}; }
    return Object.assign({ hold: 220, pause: 900, treeSound: "device", showSent: true, sound: true }, p);
  },
  save(p) { try { localStorage.setItem("smartcw.live", JSON.stringify(p)); } catch (e) { /* private mode */ } },
};

Pages.live = (() => {
  const prefs = LivePrefs.load();
  Beep.enabled = prefs.sound;
  let tree, textBig, textMorse, pendingEl, keyBtn, monitorEl, airChip, outHint;
  let text = [];          // decoded / entered characters
  let keyPat = "";        // on-screen key pattern in progress
  let padPat = "";        // paddle pattern in progress (from the device)
  let keyDownAt = 0, keyTimer = null, keyIsDown = false;
  let sentTimer = null;
  let built = false;

  function addText(c) {
    if (c === " " && (!text.length || text[text.length - 1] === " ")) return;
    text.push(c);
    if (text.length > 64) text = text.slice(text.length - 64);
    renderText();
  }

  function renderText() {
    const s = text.join("");
    textBig.textContent = s.trim() ? dispText(s) : "";
    textBig.classList.toggle("empty", !s.trim());
    if (!s.trim()) textBig.textContent = "Key, tap or type";
    textBig.style.fontSize = s.length > 36 ? "28px" : s.length > 18 ? "38px" : "";
    textMorse.replaceChildren(morseView(s.slice(-24)));
    const pat = keyPat || padPat;
    pendingEl.replaceChildren();
    if (pat) {
      for (const e of pat) pendingEl.appendChild(h(e === "." ? "i.d" : "i.h"));
      const L = decodePattern(pat);
      pendingEl.appendChild(h("b", L ? disp(L) : "?"));
    }
  }

  // ---- on-screen key ----
  function keyPress(e) {
    if (e) e.preventDefault();
    if (keyIsDown) return;
    keyIsDown = true;
    keyDownAt = performance.now();
    clearTimeout(keyTimer);
    keyBtn.classList.add("down");
    Beep.set(true);
  }
  function keyRelease() {
    if (!keyIsDown) return;
    keyIsDown = false;
    keyBtn.classList.remove("down");
    Beep.set(false);
    const dur = performance.now() - keyDownAt;
    addElement(dur < prefs.hold ? "." : "-");
  }
  function addElement(e) {
    if (keyPat.length >= 8) return;
    keyPat += e;
    tree.path(keyPat);
    renderText();
    clearTimeout(keyTimer);
    keyTimer = setTimeout(commitKey, prefs.pause);
  }
  function commitKey() {
    clearTimeout(keyTimer);
    if (!keyPat) return;
    const c = decodePattern(keyPat);
    const end = tree.path(keyPat);
    if (end) tree.flash(end, c ? "ok" : "bad");
    keyPat = "";
    addText(c || "?");
    setTimeout(() => { if (!keyPat && !padPat) tree.clear(); }, 450);
  }
  function keySpace() {
    if (keyPat) commitKey();
    else addText(" ");
  }
  function keyBack() {
    if (keyPat) {
      keyPat = keyPat.slice(0, -1);
      tree.path(keyPat);
      renderText();
      return;
    }
    text.pop();
    renderText();
  }

  // ---- device events ----
  function onElement(m) {
    if (keyPat) return;
    padPat += m.v;
    tree.path(padPat);
    renderText();
  }
  function onRx(m) {
    const c = m.c;
    if (c === " ") { addText(" "); monitor(" ", "rx"); return; }
    const end = padPat ? tree.path(padPat) : "";
    if (end) tree.flash(end, c ? "ok" : "bad");
    padPat = "";
    addText(c ? c.toUpperCase() : "?");
    monitor(c ? disp(c) : "¿", "rx");
    setTimeout(() => { if (!keyPat && !padPat) tree.clear(); }, 450);
  }
  function onTx(m) {
    const cls = m.p === 0 ? "tx" : "pr";
    monitor(m.c === " " ? " " : disp(m.c), cls);
    if (prefs.showSent && !keyPat && !padPat && m.c !== " " && Router.cur === "live") {
      tree.path(patternOf(m.c));
      clearTimeout(sentTimer);
      sentTimer = setTimeout(() => { if (!keyPat && !padPat) tree.clear(); }, 600);
    }
  }

  function monitor(s, cls) {
    let last = monitorEl.lastElementChild;
    if (!last || !last.classList.contains(cls)) {
      last = h("span." + cls);
      monitorEl.appendChild(last);
    }
    last.textContent += s;
    while (monitorEl.textContent.length > 4000 && monitorEl.firstChild) monitorEl.firstChild.remove();
    monitorEl.scrollTop = monitorEl.scrollHeight;
  }

  // ---- building ----
  function treeCard() {
    const host = h("div.tree-host");
    tree = buildTree(host, {
      onTap: (L) => {
        tree.path(MORSE[L]);
        setTimeout(() => { if (!keyPat && !padPat) tree.clear(); }, 900);
        if (prefs.treeSound === "browser") Beep.play(L, App.cfg.trainWpm || 20);
        else post("/api/send", { text: L, p: "practice" });
      },
    });
    const signs = h("div.signs");
    const groups = [["Numbers", "1234567890"], ["Signs", ".,?/=-@:;'\"()"], ["Prosigns", "+(|&^#*"]];
    for (const [title, chars] of groups) {
      signs.appendChild(h("h4", title));
      const grid = h("div.sgrid");
      for (const c of chars) {
        grid.appendChild(h("button.sg", {
          type: "button", title: "play " + disp(c),
          onclick: () => post("/api/send", { text: c, p: "practice" }),
        }, h("b", disp(c)), morseView(c)));
      }
      signs.appendChild(grid);
    }
    const pages = h("div.tpages", host, signs);
    const dots = h("div.dots",
      h("button.on", { type: "button", "aria-label": "tree", onclick: () => setPage(0) }),
      h("button", { type: "button", "aria-label": "numbers and signs", onclick: () => setPage(1) }));
    function setPage(i) {
      pages.style.transform = `translateX(${-i * 100}%)`;
      $$("button", dots).forEach((b, j) => b.classList.toggle("on", j === i));
    }
    let sx = null;
    pages.addEventListener("touchstart", (e) => { sx = e.touches[0].clientX; }, { passive: true });
    pages.addEventListener("touchend", (e) => {
      if (sx === null) return;
      const dx = e.changedTouches[0].clientX - sx;
      if (Math.abs(dx) > 60) setPage(dx < 0 ? 1 : 0);
      sx = null;
    });
    const gear = h("button.gear", { type: "button", "aria-label": "tree and key options", onclick: () => openPrefs() }, icon("gear"));
    return h("div", h("div.tree-card", h("div.tview", pages), gear), dots);
  }

  function openPrefs() {
    const num = (k, min, max, step, label, help) => {
      const out = h("output", prefs[k] + " ms");
      const inp = h("input", {
        type: "range", min, max, step, value: prefs[k],
        oninput: (e) => { prefs[k] = Number(e.target.value); out.textContent = prefs[k] + " ms"; LivePrefs.save(prefs); },
      });
      return row(label, help, h("div.rng", inp, out));
    };
    const treeSnd = h("div.seg", ...["device", "browser"].map((v) => h("button", {
      type: "button", "aria-pressed": String(prefs.treeSound === v),
      onclick: (e) => {
        prefs.treeSound = v;
        LivePrefs.save(prefs);
        $$("button", e.target.parentNode).forEach((b) => b.setAttribute("aria-pressed", String(b === e.target)));
      },
    }, v === "device" ? "SmartCW" : "Browser")));
    const sent = h("input", { type: "checkbox", onchange: (e) => { prefs.showSent = e.target.checked; LivePrefs.save(prefs); } });
    sent.checked = prefs.showSent;
    modal("Tree and key", [
      num("hold", 120, 450, 10, "Hold for a dash", "a tap shorter than this is a dot"),
      num("pause", 400, 2500, 50, "Letter pause", "this long without a tap ends the letter"),
      row("Tree taps sound on", "the letter you tap is played here", treeSnd),
      row("Show sent text on the tree", "follow what SmartCW sends", h("label.sw", sent, h("i"))),
    ]);
  }

  function textCard() {
    textBig = h("div.big");
    textMorse = h("div.mline");
    pendingEl = h("div.pending");
    const spk = h("button.ib", { type: "button", "aria-label": "browser sound", onclick: () => {
      Beep.enabled = !Beep.enabled;
      prefs.sound = Beep.enabled;
      LivePrefs.save(prefs);
      spk.replaceChildren(icon(Beep.enabled ? "speaker" : "mute"));
      spk.classList.toggle("off", !Beep.enabled);
    } }, icon(Beep.enabled ? "speaker" : "mute"));
    spk.classList.toggle("off", !Beep.enabled);
    const replay = h("button.ib", { type: "button", "aria-label": "play the text", onclick: () => {
      const s = text.join("").trim();
      if (!s) return;
      post("/api/send", { text: s, p: "practice" });
    } }, icon("replay"));
    const clear = h("button.ib", { type: "button", "aria-label": "clear", onclick: () => { text = []; renderText(); } }, icon("trash"));
    return h("div.text-card", h("div.tc-main", textBig, textMorse, pendingEl), h("div.tc-side", spk, replay, clear));
  }

  function keyRow() {
    keyBtn = h("button.key", { type: "button", "aria-label": "Morse key: tap for a dot, hold for a dash" }, h("span.knob"));
    keyBtn.addEventListener("pointerdown", (e) => { try { keyBtn.setPointerCapture(e.pointerId); } catch (err) { /* synthetic */ } keyPress(e); });
    keyBtn.addEventListener("pointerup", keyRelease);
    keyBtn.addEventListener("pointercancel", keyRelease);
    keyBtn.addEventListener("contextmenu", (e) => e.preventDefault());
    return h("div",
      h("div.keyrow",
        h("button.kside", { type: "button", "aria-label": "space", onclick: keySpace }, icon("space")),
        keyBtn,
        h("button.kside", { type: "button", "aria-label": "backspace", onclick: keyBack }, icon("backspace"))),
      h("div.keycap", h("span", "TAP · DOT"), h("span", "HOLD — DASH")));
  }

  function modeCard() {
    outHint = h("p.hint");
    const kbd = switchInput("keyboard");
    const body = [
      h("div.fld", h("label", "Key type"),
        segmented("keyer", ["iambicA", "iambicB", "ultimatic", "bug", "straight"], ["Iambic A", "Iambic B", "Ultimatic", "Bug", "Straight"])),
      h("div.fld", h("label", "Transmit output"),
        segmented("output", ["key", "audio", "off"], ["Key line", "Audio + PTT", "Practice"]), outHint),
      h("div.steps", stepper("wpm", "WPM", 1), stepper("tone", "TONE", 10, " Hz"), stepper("farnsworth", "FARNSWORTH", 1)),
      h("div.toggles",
        h("label.check", switchInput("buzzer"), " Buzzer"),
        h("label.check", kbd, " Keyboard typing", h("small", " (restart)")),
        h("label.check", switchInput("midi"), " MIDI", h("small", " (restart)"))),
    ];
    return card("sliders", "Mode", "How SmartCW keys and what it transmits", ...body);
  }

  function sendCard() {
    const ta = h("textarea", { rows: 2, placeholder: "Type text to send, Enter sends. <SK> <KN> <AR> <BT> for prosigns", spellcheck: "false" });
    const liveChk = h("input", { type: "checkbox" });
    let liveSent = 0;
    const send = () => {
      const t = ta.value.trim();
      if (!t) return;
      post("/api/send", { text: t + " " });
      ta.value = "";
      liveSent = 0;
    };
    ta.addEventListener("keydown", (e) => {
      if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); if (liveChk.checked) { post("/api/send", { text: " " }); ta.value = ""; liveSent = 0; } else send(); }
      if (e.key === "Escape") post("/api/stop");
    });
    ta.addEventListener("input", () => {
      if (!liveChk.checked) return;
      const v = ta.value;
      if (v.length > liveSent) post("/api/send", { text: v.slice(liveSent) });
      liveSent = v.length;
    });
    const tuneBtn = h("button", { type: "button", onclick: () => post("/api/tune", { ms: App.rt.tune ? 0 : 5000 }) }, icon("tune"), "Tune 5 s");
    const beaconBtn = h("button", { type: "button", onclick: () => post("/api/beacon", { on: App.rt.beacon ? 0 : 1 }) }, icon("beacon"), "Beacon");
    const mems = h("div.mems");
    const syncMem = () => {
      mems.replaceChildren();
      for (let i = 1; i <= 6; i++) {
        const raw = App.cfg["mem" + i] || "";
        const t = expandShow(raw.toUpperCase());
        mems.appendChild(h("button.mem", { type: "button", disabled: !expand(raw).trim(), title: t, onclick: () => post("/api/mem", { n: i }) },
          h("small", "M" + i), h("span", t || "empty")));
      }
      tuneBtn.classList.toggle("on", !!App.rt.tune);
      beaconBtn.classList.toggle("on", !!App.rt.beacon);
    };
    Bus.on("state", syncMem);
    return card("cw", "Send", "Text, memories, beacon and tune",
      ta,
      h("div.actions.tight",
        h("button.primary", { type: "button", onclick: send }, icon("send"), "Send"),
        h("button", { type: "button", onclick: () => post("/api/stop") }, icon("stop"), "Stop"),
        tuneBtn, beaconBtn,
        h("button", { type: "button", onclick: () => post("/api/ptt") }, "PTT"),
        h("label.check.small", liveChk, " send while typing")),
      mems,
      h("p.hint", "Memories, beacon text and speed are set in ", h("a", { href: "#/settings" }, "Settings"), "."));
  }

  function monitorCard() {
    monitorEl = h("div.monitor", { "aria-live": "polite" });
    airChip = h("span.air", "ON AIR");
    return card("list", "Monitor", "Paddles in white, sent text in amber",
      monitorEl,
      h("div.actions.tight", airChip, h("span.legend", h("i.rx"), "paddles ", h("i.tx"), "sent ", h("i.pr"), "practice"),
        h("button.sm", { type: "button", onclick: () => monitorEl.replaceChildren(), style: { marginLeft: "auto" } }, "Clear")));
  }

  function build() {
    const sec = $("#p-live");
    sec.append(
      h("div.page-head", h("h2", icon("cw"), "Live"),
        h("p.sub", "Key with your paddles or straight key, tap the key below, or type text to send. The tree follows every dit and dah.")),
      h("div.live-grid",
        h("div.col-a", treeCard(), textCard(), keyRow()),
        h("div.col-b", modeCard(), sendCard(), monitorCard())));
    renderText();

    Bus.on("el", onElement);
    Bus.on("rx", onRx);
    Bus.on("tx", onTx);
    Bus.on("abort", () => monitor(" [stopped] ", "pr"));
    const air = () => airChip.classList.toggle("on", App.txDown);
    Bus.on("k", air);
    Bus.on("ptt", (m) => airChip.classList.toggle("ptt", !!m.d));
    Bus.on("state", () => {
      const o = App.cfg.output;
      outHint.textContent = App.rt.practice ? "A trainer page holds practice mode: nothing is transmitted right now."
        : o === "key" ? "The output jack keys your radio like a straight key."
        : o === "audio" ? `The output jack is PTT (${App.cfg.pttLead} ms lead, ${App.cfg.pttTail} ms tail), the tone goes out on line out.`
        : "Practice: sidetone only, nothing is transmitted.";
    });

    document.addEventListener("keydown", (e) => {
      if (Router.cur !== "live" || e.target.closest("input,textarea,select") || e.ctrlKey || e.metaKey || e.altKey) return;
      if (e.key === "." ) { addElement("."); e.preventDefault(); }
      else if (e.key === "-") { addElement("-"); e.preventDefault(); }
      else if (e.key === " " || e.key === "Enter") { keySpace(); e.preventDefault(); }
      else if (e.key === "Backspace") { keyBack(); e.preventDefault(); }
      else if (e.key === "Escape") post("/api/stop");
    });
  }

  return {
    show() { if (!built) { build(); built = true; } },
  };
})();

// Small modal used for options.
function modal(title, body) {
  const box = h("div.spop", { onclick: (e) => { if (e.target === box) box.remove(); } },
    h("div.sbox", h("h3", title), h("div.mbody", body),
      h("div.actions", h("button.primary", { type: "button", onclick: () => box.remove() }, "Done"))));
  document.body.appendChild(box);
  const esc = (e) => { if (e.key === "Escape") { box.remove(); document.removeEventListener("keydown", esc); } };
  document.addEventListener("keydown", esc);
  return box;
}
