/* Trainer: echo (iambic), Koch copy, own call, callsigns, words, codes,
 * QSO practice, head copy, fist analyzer and progress. The trainer pages hold
 * practice mode, so nothing is transmitted while they are open. */
"use strict";

const TPrefs = {
  d: null,
  load() {
    let p = {};
    try { p = JSON.parse(localStorage.getItem("smartcw.train") || "{}"); } catch (e) { p = {}; }
    this.d = Object.assign({
      sound: "device", echoGroup: 1, echoPool: "koch", reveal: false, copyGroup: 3, copyPool: "koch",
      callMode: "hear", wordSet: "words", wordMode: "hear", codeMode: "hear", codeSet: "all", showQso: false,
    }, p);
    return this.d;
  },
  set(k, v) { this.d[k] = v; try { localStorage.setItem("smartcw.train", JSON.stringify(this.d)); } catch (e) { /* */ } },
};

const Stats = {
  d: null,
  load() {
    try { this.d = JSON.parse(localStorage.getItem("smartcw.stats") || "null"); } catch (e) { this.d = null; }
    if (!this.d || typeof this.d !== "object") this.d = {};
    this.d = Object.assign({ c: {}, codes: {}, koch: 2, best: {}, total: 0 }, this.d);
    return this.d;
  },
  save() { try { localStorage.setItem("smartcw.stats", JSON.stringify(this.d)); } catch (e) { /* */ } },
  rec(ch, ok) {
    if (!ch || ch === " ") return;
    const c = (this.d.c[ch] = this.d.c[ch] || { n: 0, ok: 0, r: [] });
    c.n++;
    if (ok) c.ok++;
    c.r.push(ok ? 1 : 0);
    if (c.r.length > 20) c.r.shift();
    this.d.total++;
    this.save();
  },
  recent(ch) {
    const c = this.d.c[ch];
    if (!c || !c.r.length) return null;
    return c.r.reduce((a, b) => a + b, 0) / c.r.length;
  },
  weight(ch) {
    const r = this.recent(ch);
    const c = this.d.c[ch];
    if (r === null || c.r.length < 3) return 3;
    return 1 + 5 * (1 - r);
  },
  best(key, v) {
    if (v > (this.d.best[key] || 0)) { this.d.best[key] = v; this.save(); return true; }
    return false;
  },
};

function sleep(ms) { return new Promise((r) => setTimeout(r, ms)); }

function poolChars(name) {
  if (name === "letters") return "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("");
  if (name === "alnum") return "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".split("");
  if (name === "all") return KOCH.slice();
  return KOCH.slice(0, Math.max(2, Stats.d.koch));
}

function pickWeighted(chars) {
  const newest = chars[chars.length - 1];
  const w = chars.map((c) => Stats.weight(c) * (c === newest && TPrefs.d.echoPool === "koch" ? 2 : 1));
  let r = Math.random() * w.reduce((a, b) => a + b, 0);
  for (let i = 0; i < chars.length; i++) { r -= w[i]; if (r <= 0) return chars[i]; }
  return chars[chars.length - 1];
}

function levenshtein(a, b) {
  const m = a.length, n = b.length;
  const d = Array.from({ length: m + 1 }, (_, i) => [i]);
  for (let j = 1; j <= n; j++) d[0][j] = j;
  for (let i = 1; i <= m; i++) for (let j = 1; j <= n; j++)
    d[i][j] = Math.min(d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
  return d[m][n];
}

Pages.train = (() => {
  let token = 0;
  let held = false;
  let view = null;

  const alive = (t) => t === token;

  function stopSession() {
    token++;
    post("/api/stop").catch(() => {});
  }

  // Play text with the trainer speed; resolves when it has been sent.
  function playTrain(text, opts = {}) {
    const wpm = opts.wpm || App.cfg.trainWpm || 20;
    const eff = opts.eff !== undefined ? opts.eff : App.cfg.trainEff || 0;
    if (TPrefs.d.sound === "browser") {
      return Beep.play(text, wpm, App.cfg.trainTone || App.cfg.tone, eff);
    }
    return new Promise((res) => {
      const units = normalize(text).length * 14 + 10;
      const est = (units * 1200) / Math.max(5, Math.min(wpm, eff || wpm)) + 4000;
      let done = false;
      const fin = () => { if (done) return; done = true; Bus.off("done", fin); Bus.off("abort", fin); clearTimeout(t); res(); };
      const t = setTimeout(fin, est);
      Bus.on("done", fin);
      Bus.on("abort", fin);
      const params = { text, p: "train" };
      if (opts.wpm) { params.wpm = opts.wpm; params.eff = eff; }
      post("/api/send", params).catch(fin);
    });
  }

  // Collect characters keyed on the paddles / straight key.
  function captureKeyed(o) {
    const opt = Object.assign({ expect: 0, idleMs: 2500, firstMs: 8000, maxMs: 20000, tok: token, onChar: null, stop: null }, o);
    return new Promise((res) => {
      let got = "", last = Date.now(), started = false;
      const t0 = Date.now();
      const onEl = () => { started = true; last = Date.now(); };
      const onRx = (m) => {
        last = Date.now();
        started = true;
        got += m.c === "" ? "?" : m.c === " " ? " " : m.c.toUpperCase();
        if (opt.onChar) opt.onChar(got);
        if (opt.expect && got.replace(/ /g, "").length >= opt.expect) finish();
      };
      const timer = setInterval(() => {
        const now = Date.now();
        if (!alive(opt.tok)) finish();
        else if (!started && now - t0 > opt.firstMs) finish();
        else if (started && now - last > opt.idleMs) finish();
        else if (now - t0 > opt.maxMs) finish();
      }, 100);
      let finished = false;
      function finish() {
        if (finished) return;
        finished = true;
        clearInterval(timer);
        Bus.off("el", onEl);
        Bus.off("rx", onRx);
        res(got.replace(/\s+/g, " ").trim());
      }
      if (opt.stop) opt.stop.finish = finish;
      Bus.on("el", onEl);
      Bus.on("rx", onRx);
    });
  }

  // ---------- shared pieces ----------
  function speedBar() {
    const snd = h("div.seg", ...["device", "browser"].map((v) => h("button", {
      type: "button", "aria-pressed": String(TPrefs.d.sound === v),
      onclick: (e) => {
        TPrefs.set("sound", v);
        $$("button", e.target.parentNode).forEach((b) => b.setAttribute("aria-pressed", String(b === e.target)));
      },
    }, v === "device" ? "SmartCW" : "Browser")));
    return h("div.tbar",
      stepper("trainWpm", "CHAR WPM", 1),
      stepper("trainEff", "EFFECTIVE", 1, { fmt: (v) => (v ? v : "off"), zeroBelow: 5, zeroStart: 10 }),
      stepper("trainTone", "TONE", 10, { fmt: (v) => (v ? v + " Hz" : "keyer"), zeroBelow: 300, zeroStart: 600 }),
      h("div.tsnd", h("small", "SOUND"), snd),
      h("span.chip.mute", icon("lock"), "TX muted while training"));
  }

  function stage() {
    const big = h("div.tr-big", "");
    const morse = h("div.tr-morse");
    const msg = h("div.tr-msg");
    const el = h("div.tr-stage", big, morse, msg);
    return {
      el,
      set(state, bigText, morseText, message) {
        el.className = "tr-stage" + (state ? " " + state : "");
        if (bigText !== undefined) big.textContent = bigText;
        if (morseText !== undefined) morse.replaceChildren(morseText ? morseView(morseText) : "");
        if (message !== undefined) {
          if (message instanceof Node) msg.replaceChildren(message); else msg.textContent = message;
        }
      },
    };
  }

  function segPref(key, options, labels, onChange) {
    return h("div.seg", ...options.map((o, i) => h("button", {
      type: "button", "aria-pressed": String(TPrefs.d[key] === o),
      onclick: (e) => {
        TPrefs.set(key, o);
        $$("button", e.target.parentNode).forEach((b) => b.setAttribute("aria-pressed", String(b === e.target)));
        if (onChange) onChange(o);
      },
    }, labels[i])));
  }

  function scoreChips() {
    const box = h("div.tr-score");
    return {
      el: box,
      set(items) { box.replaceChildren(...items.map(([k, v, cls]) => h("span.chip" + (cls ? "." + cls : ""), h("small", k), " ", h("b", String(v))))); },
    };
  }

  function startStop(run) {
    let running = false;
    const btn = h("button.primary", { type: "button" }, icon("play"), "Start");
    const set = (r) => {
      running = r;
      btn.replaceChildren(icon(r ? "stop" : "play"), r ? "Stop" : "Start");
      btn.classList.toggle("primary", !r);
    };
    btn.addEventListener("click", () => {
      if (running) { stopSession(); set(false); return; }
      token++;
      const t = token;
      set(true);
      Promise.resolve(run(t)).catch((e) => showFatal(e.message)).finally(() => { if (alive(t)) set(false); });
    });
    return btn;
  }

  function needIambic() {
    const ok = App.cfg.keyer === "iambicA" || App.cfg.keyer === "iambicB";
    if (ok) return null;
    return h("div.warn", "Listen and repeat works with the iambic keyer. ",
      h("button.sm", { type: "button", onclick: () => { Settings.set("keyer", "iambicB"); setTimeout(() => render("echo"), 400); } }, "Switch to Iambic B"));
  }

  function needCall() {
    if (App.cfg.call) return null;
    const inp = h("input", { type: "text", placeholder: "your callsign", maxlength: 15, style: { width: "12em" } });
    return h("div.warn", "Set your callsign first: ", inp,
      h("button.sm", { type: "button", onclick: () => { if (inp.value.trim()) { Settings.set("call", inp.value.trim().toUpperCase()); setTimeout(() => render(view), 500); } } }, "Save"));
  }

  // ---------- 1. listen and repeat (iambic) ----------
  function viewEcho() {
    const st = stage();
    const sc = scoreChips();
    let results = [], streak = 0;
    const update = () => {
      const last = results.slice(-20);
      const acc = last.length ? Math.round((100 * last.filter((x) => x).length) / last.length) : 0;
      sc.set([["level", TPrefs.d.echoPool === "koch" ? Stats.d.koch + " chars" : TPrefs.d.echoPool], ["streak", streak], ["last 20", acc + "%"],
        ["new", TPrefs.d.echoPool === "koch" ? disp(KOCH[Stats.d.koch - 1]) : "-"]]);
    };
    st.set("", "?", "", "Press Start. SmartCW plays a character, you key it back on the paddles.");
    update();
    const run = async (t) => {
      while (alive(t)) {
        const pool = poolChars(TPrefs.d.echoPool);
        let group = "";
        for (let i = 0; i < TPrefs.d.echoGroup; i++) group += pickWeighted(pool);
        st.set("listen", TPrefs.d.reveal ? dispText(group) : "?", TPrefs.d.reveal ? group : "", "Listen…");
        await playTrain(group);
        if (!alive(t)) return;
        st.set("turn", undefined, undefined, "Your turn: key it back");
        const got = (await captureKeyed({ expect: group.length, idleMs: 2200, firstMs: 7000, tok: t })).replace(/ /g, "");
        if (!alive(t)) return;
        const ok = got === group;
        for (let i = 0; i < group.length; i++) Stats.rec(group[i], got[i] === group[i]);
        results.push(ok);
        if (ok) {
          streak++;
          st.set("ok", dispText(group), group, "✓ Correct");
        } else {
          streak = 0;
          st.set("bad", dispText(group), group, got ? "You sent " + dispText(got) + ", listen again" : "Nothing keyed, listen again");
          await sleep(500);
          if (!alive(t)) return;
          await playTrain(group);
        }
        // Koch: 90 % over the last 20 at this level unlocks the next character
        if (TPrefs.d.echoPool === "koch" && results.length >= 20) {
          const last = results.slice(-20);
          if (last.filter((x) => x).length >= 18 && Stats.d.koch < KOCH.length) {
            Stats.d.koch++;
            Stats.save();
            results = [];
            toast("New character: " + disp(KOCH[Stats.d.koch - 1]) + "  " + patternOf(KOCH[Stats.d.koch - 1]), "ok");
          }
        }
        update();
        await sleep(ok ? 650 : 1100);
      }
    };
    return [
      needIambic(),
      st.el,
      h("div.actions", startStop(run),
        h("label", "Group"), segPref("echoGroup", [1, 2, 3], ["1", "2", "3"]),
        h("label", "Characters"), segPref("echoPool", ["koch", "letters", "alnum", "all"], ["Koch", "A-Z", "A-Z 0-9", "All"], update),
        h("label.check.small", (() => { const c = h("input", { type: "checkbox", onchange: (e) => TPrefs.set("reveal", e.target.checked) }); c.checked = TPrefs.d.reveal; return c; })(), " show it first")),
      sc.el,
      h("p.hint", "Koch method: start with K and M, a new character is added when you get 18 of the last 20 right. Characters you miss come back more often."),
    ];
  }

  // ---------- 2. copy characters (receive, Koch) ----------
  function viewCopy() {
    const st = stage();
    const sc = scoreChips();
    const ans = h("input.tr-answer", { type: "text", autocomplete: "off", spellcheck: "false", placeholder: "type what you hear" });
    let tries = 0, good = 0;
    const update = () => sc.set([["level", Stats.d.koch + " chars"], ["groups", tries], ["correct", tries ? Math.round((100 * good) / tries) + "%" : "-"]]);
    st.set("", "", "", "Press Start, listen to a group and type it.");
    update();
    const run = async (t) => {
      while (alive(t)) {
        const pool = poolChars(TPrefs.d.copyPool);
        let group = "";
        for (let i = 0; i < TPrefs.d.copyGroup; i++) group += pickWeighted(pool);
        ans.value = "";
        st.set("listen", "", "", "Listen…");
        await playTrain(group);
        if (!alive(t)) return;
        st.set("turn", undefined, undefined, "Type it, Enter to check (Enter on empty = repeat)");
        ans.focus();
        let typed = null;
        while (typed === null && alive(t)) {
          typed = await new Promise((res) => {
            const k = (e) => {
              if (e.key !== "Enter") return;
              ans.removeEventListener("keydown", k);
              res(ans.value);
            };
            ans.addEventListener("keydown", k);
            const iv = setInterval(() => { if (!alive(t)) { clearInterval(iv); ans.removeEventListener("keydown", k); res(""); } }, 200);
          });
          if (alive(t) && typed.trim() === "") { typed = null; await playTrain(group); }
        }
        if (!alive(t)) return;
        const got = normalize(typed).replace(/\s/g, "");
        tries++;
        let ok = true;
        for (let i = 0; i < group.length; i++) {
          const r = got[i] === group[i];
          Stats.rec(group[i], r);
          if (!r) ok = false;
        }
        if (ok && got.length === group.length) good++;
        else ok = false;
        st.set(ok ? "ok" : "bad", dispText(group), group, ok ? "✓ Correct" : "You typed " + (dispText(got) || "nothing"));
        update();
        await sleep(ok ? 800 : 1800);
      }
    };
    return [
      st.el, ans,
      h("div.actions", startStop(run),
        h("label", "Group"), segPref("copyGroup", [1, 3, 5], ["1", "3", "5"]),
        h("label", "Characters"), segPref("copyPool", ["koch", "letters", "alnum", "all"], ["Koch", "A-Z", "A-Z 0-9", "All"])),
      sc.el,
      h("p.hint", "The Koch level is shared with Listen and repeat. Use a high character speed with Farnsworth spacing, e.g. 20 wpm at 10 effective."),
    ];
  }

  // ---------- 3/4/5. item trainers: own call, callsigns, words ----------
  function itemTrainer(cfg) {
    const st = stage();
    const sc = scoreChips();
    const ans = h("input.tr-answer", { type: "text", autocomplete: "off", spellcheck: "false", placeholder: "type what you hear" });
    let wpm = App.cfg.trainWpm || 20;
    let n = 0, good = 0, streak = 0;
    const mode = () => TPrefs.d[cfg.modeKey];
    const update = () => {
      const items = [["done", n], ["correct", n ? Math.round((100 * good) / n) + "%" : "-"], ["streak", streak]];
      if (cfg.ramp && mode() === "hear") items.push(["speed", wpm + " wpm"], ["best", (Stats.d.best[cfg.id] || 0) + " wpm"]);
      sc.set(items);
      ans.hidden = mode() !== "hear";
    };
    st.set("", "", "", cfg.intro);
    const run = async (t) => {
      wpm = App.cfg.trainWpm || 20;
      while (alive(t)) {
        const item = normalize(cfg.next());
        if (!item) return;
        let ok;
        if (mode() === "hear") {
          ans.value = "";
          st.set("listen", "", "", "Listen…");
          await playTrain(item, cfg.ramp ? { wpm } : {});
          if (!alive(t)) return;
          st.set("turn", undefined, undefined, "Type it, Enter to check (Enter on empty = repeat)");
          ans.focus();
          let typed = null;
          while (typed === null && alive(t)) {
            typed = await new Promise((res) => {
              const k = (e) => { if (e.key === "Enter") { ans.removeEventListener("keydown", k); res(ans.value); } };
              ans.addEventListener("keydown", k);
              const iv = setInterval(() => { if (!alive(t)) { clearInterval(iv); ans.removeEventListener("keydown", k); res(""); } }, 200);
            });
            if (alive(t) && !typed.trim()) { typed = null; await playTrain(item, cfg.ramp ? { wpm } : {}); }
          }
          if (!alive(t)) return;
          ok = normalize(typed).replace(/\s+/g, " ").trim() === item;
          st.set(ok ? "ok" : "bad", dispText(item), item, ok ? "✓ Correct" : "You typed " + (dispText(typed) || "nothing"));
          if (cfg.ramp) {
            if (ok) { wpm = Math.min(60, wpm + 2); if (Stats.best(cfg.id, wpm)) toast("New best: " + wpm + " wpm", "ok"); }
            else wpm = Math.max(5, wpm - 2);
          }
        } else {
          st.set("turn", dispText(item), item, "Key it on the paddles or key");
          const got = await captureKeyed({ expect: item.replace(/ /g, "").length, idleMs: 2500, firstMs: 15000, tok: t,
            onChar: (g) => st.set("turn", undefined, undefined, "You: " + dispText(g)) });
          if (!alive(t)) return;
          ok = got.replace(/ /g, "") === item.replace(/ /g, "");
          st.set(ok ? "ok" : "bad", dispText(item), item, ok ? "✓ Clean" : "You sent " + (dispText(got) || "nothing"));
        }
        n++;
        if (ok) { good++; streak++; } else streak = 0;
        for (const c of item.replace(/ /g, "")) Stats.rec(c, ok);
        update();
        await sleep(ok ? 800 : 1700);
      }
    };
    update();
    return [
      cfg.pre ? cfg.pre() : null,
      st.el, ans,
      h("div.actions", startStop(run),
        segPref(cfg.modeKey, ["hear", "send"], ["Hear and type", "See and key"], update),
        cfg.extra ? cfg.extra() : null),
      sc.el,
      cfg.hint ? h("p.hint", cfg.hint) : null,
    ];
  }

  function viewMyCall() {
    return itemTrainer({
      id: "mycall", modeKey: "callMode", ramp: true,
      intro: "Your own call, until it sounds like a word. Hear it get faster with every right answer, or key it cleanly.",
      next: () => App.cfg.call,
      pre: needCall,
      hint: "Every correct copy adds 2 wpm, a miss takes 2 off. Your best speed is kept in this browser.",
    });
  }

  function viewCalls() {
    return itemTrainer({
      id: "calls", modeKey: "callMode", ramp: true,
      intro: "Random callsigns from busy prefixes. Speed climbs with every correct one.",
      next: () => randomCall().call,
      hint: "Like a pile-up: calls are made from real prefixes, with /P now and then.",
    });
  }

  function viewWords() {
    const sets = {
      words: () => pick(COMMON_WORDS),
      abbr: () => pick(ABBREVIATIONS)[0],
      q: () => pick(QCODES)[0],
      mix: () => pick([pick(COMMON_WORDS), pick(ABBREVIATIONS)[0], pick(QCODES)[0]]),
    };
    return itemTrainer({
      id: "words", modeKey: "wordMode", ramp: false,
      intro: "Common words, ham abbreviations and Q-codes, heard as whole words.",
      next: () => (sets[TPrefs.d.wordSet] || sets.words)(),
      extra: () => segPref("wordSet", ["words", "abbr", "q", "mix"], ["Words", "Abbreviations", "Q-codes", "Mix"]),
      hint: "Copy whole words instead of letter by letter: that is what makes CW conversational.",
    });
  }

  // ---------- 6. codes: Q-codes, abbreviations, prosigns ----------
  function viewCodes() {
    const st = stage();
    const sc = scoreChips();
    const opts = h("div.choices");
    let n = 0, good = 0;
    const deck = () => {
      const s = TPrefs.d.codeSet;
      const q = QCODES.map(([c, ask, tell]) => ({ code: c, mean: tell, alt: ask }));
      const a = ABBREVIATIONS.map(([c, m]) => ({ code: c, mean: m }));
      const p = PROSIGNS.map(([c, ch, m]) => ({ code: c, mean: m }));
      return s === "q" ? q : s === "abbr" ? a : s === "pro" ? p : q.concat(a, p);
    };
    const update = () => sc.set([["asked", n], ["correct", n ? Math.round((100 * good) / n) + "%" : "-"]]);
    st.set("", "", "", "Hear a code and pick its meaning, or see the meaning and key the code.");
    const run = async (t) => {
      while (alive(t)) {
        const d = deck();
        const item = pick(d);
        let ok;
        if (TPrefs.d.codeMode === "hear") {
          const choices = [item];
          while (choices.length < 4) { const c = pick(d); if (!choices.includes(c) && c.mean !== item.mean) choices.push(c); }
          choices.sort(() => Math.random() - 0.5);
          st.set("listen", "", "", "Listen…");
          opts.replaceChildren();
          await playTrain(item.code);
          if (!alive(t)) return;
          st.set("turn", undefined, undefined, "What does it mean?");
          const chosen = await new Promise((res) => {
            opts.replaceChildren(...choices.map((c) => h("button.choice", { type: "button", onclick: () => res(c) }, c.mean)),
              h("button.choice.again", { type: "button", onclick: () => playTrain(item.code) }, icon("replay"), "again"));
            const iv = setInterval(() => { if (!alive(t)) { clearInterval(iv); res(null); } }, 200);
          });
          if (!alive(t) || !chosen) return;
          ok = chosen === item;
          $$("button.choice", opts).forEach((b) => {
            if (b.textContent === item.mean) b.classList.add("right");
            else if (b.textContent === chosen.mean && !ok) b.classList.add("wrong");
          });
          st.set(ok ? "ok" : "bad", dispText(item.code), item.code, (ok ? "✓ " : "") + item.code + ": " + item.mean);
        } else {
          opts.replaceChildren();
          st.set("turn", "", "", h("span", h("b", item.mean), " — key the code"));
          const target = normalize(item.code);
          const got = await captureKeyed({ expect: target.replace(/ /g, "").length, idleMs: 2500, firstMs: 15000, tok: t });
          if (!alive(t)) return;
          ok = got.replace(/ /g, "") === target.replace(/ /g, "");
          st.set(ok ? "ok" : "bad", dispText(item.code), item.code, ok ? "✓ " + item.mean : "You sent " + (dispText(got) || "nothing"));
        }
        n++;
        if (ok) good++;
        const cs = (Stats.d.codes[item.code] = Stats.d.codes[item.code] || { n: 0, ok: 0 });
        cs.n++;
        if (ok) cs.ok++;
        Stats.save();
        update();
        await sleep(ok ? 1200 : 2400);
      }
    };
    update();
    const table = (title, rows) => h("details.ref", h("summary", title),
      h("div.table-wrap", h("table", h("tbody", ...rows.map((r) => h("tr",
        h("td", h("button.sm", { type: "button", "aria-label": "play " + r[0], onclick: () => post("/api/send", { text: r[0], p: "practice" }) }, icon("play"))),
        h("td", h("b", r[0])), h("td.dim", morseView(r[0])), h("td", r[1])))))));
    return [
      st.el, opts,
      h("div.actions", startStop(run),
        segPref("codeMode", ["hear", "send"], ["Hear and pick", "Read and key"]),
        segPref("codeSet", ["all", "q", "abbr", "pro"], ["All", "Q-codes", "Abbreviations", "Prosigns"])),
      sc.el,
      table("Q-codes", QCODES.map(([c, ask, tell]) => [c, tell + " / " + ask])),
      table("Abbreviations", ABBREVIATIONS),
      table("Prosigns", PROSIGNS.map(([c, ch, m]) => [c, m])),
    ];
  }

  // ---------- 7. QSO practice ----------
  const END = /(\bK\b|\(|\+|\^|\bKN\b|\bBK\b)\s*$/;
  const RST = /\b[1-5][1-9N][1-9N]\b/;
  const SK = /(\||\bSK\b|\bVA\b)/;
  function has(word) { return new RegExp("(^|[^A-Z0-9/])" + word.replace(/[/]/g, "\\/") + "($|[^A-Z0-9/])"); }

  function scenarios(me, dx) {
    const myName = App.cfg.name ? App.cfg.name.toUpperCase() : "OP";
    return {
      answer: {
        title: "Answer a CQ", desc: "Someone calls CQ: answer, exchange report, name and QTH, then close.",
        steps: [
          { they: `CQ CQ CQ DE ${dx.call} ${dx.call} K`, task: "Answer: their call, DE, your call twice, K",
            checks: [["their call", has(dx.call)], ["DE", has("DE")], ["your call", has(me)], ["end with K", END]],
            model: `${dx.call} DE ${me} ${me} K` },
          { they: `${me} DE ${dx.call} GM TNX FER CALL = UR RST ${dx.rst} ${dx.rst} = NAME ${dx.name} ${dx.name} = QTH ${dx.qth} = HW? <AR> ${me} DE ${dx.call} <KN>`,
            task: "Reply: report, your name and QTH, hand back with <KN>",
            checks: [["their call", has(dx.call)], ["your call", has(me)], ["a report like 579 or 5NN", RST], ["NAME", has("NAME")], ["QTH", has("QTH")], ["hand back with <KN> or K", END]],
            model: `${dx.call} DE ${me} R TNX ${dx.name} = UR RST 579 579 = NAME ${myName} ${myName} = QTH ... = <AR> ${dx.call} DE ${me} <KN>` },
          { they: `${me} DE ${dx.call} R FB ${myName} TNX FER QSO = 73 ES GL <SK> ${me} DE ${dx.call} TU`,
            task: "Close the contact: thanks, 73, <SK>",
            checks: [["their call", has(dx.call)], ["73", has("73")], ["<SK>", SK]],
            model: `${dx.call} DE ${me} TNX FER QSO 73 <SK> TU` },
          { they: "E E", done: true },
        ],
      },
      cq: {
        title: "Call CQ", desc: "You call CQ, a station answers, you run the contact.",
        steps: [
          { they: null, task: "Call CQ: CQ three times, DE, your call twice, K",
            checks: [["CQ", has("CQ")], ["DE", has("DE")], ["your call", has(me)], ["end with K", END]],
            model: `CQ CQ CQ DE ${me} ${me} K` },
          { they: `${me} DE ${dx.call} ${dx.call} K`, task: "They answered: their call, DE, your call, a report, hand back",
            checks: [["their call", has(dx.call)], ["your call", has(me)], ["a report", RST], ["hand back", END]],
            model: `${dx.call} DE ${me} GM TNX FER CALL = UR RST 599 599 = NAME ${myName} = HW? <AR> ${dx.call} DE ${me} <KN>` },
          { they: `${me} DE ${dx.call} R TNX UR RST ${dx.rst} = NAME ${dx.name} = QTH ${dx.qth} = 73 <SK> ${me} DE ${dx.call}`,
            task: "They are closing: thank them, 73, <SK>",
            checks: [["their call", has(dx.call)], ["73", has("73")], ["<SK>", SK]],
            model: `${dx.call} DE ${me} R TNX ${dx.name} 73 <SK> TU` },
          { they: "TU E E", done: true },
        ],
      },
      contest: {
        title: "Contest", desc: "Fast and short: your call, the report and a serial number.",
        steps: [
          { they: `CQ TEST ${dx.call}`, task: "Send your call, once, nothing else",
            checks: [["your call", has(me)], ["only your call", new RegExp("^" + me.replace(/[/]/g, "\\/") + "$")]], model: me },
          { they: `${me} 5NN ${dx.nr}`, task: "Send TU, 5NN and your serial number 001",
            checks: [["5NN", has("5NN")], ["a serial number", /\b[0-9T]{1,4}\b/]], model: "TU 5NN 001" },
          { they: `TU ${dx.call} TEST`, done: true },
        ],
      },
      pota: {
        title: "Park hunter", desc: "A POTA activator calls CQ: a short, friendly exchange.",
        steps: [
          { they: `CQ POTA CQ POTA DE ${dx.call} ${dx.call} K`, task: "Call once with your call",
            checks: [["your call", has(me)]], model: me },
          { they: `${me} GM UR 5NN 5NN <BK>`, task: "Return the report: <BK> TU UR 5NN 5NN 73 <BK>",
            checks: [["a report", RST], ["73", has("73")], ["<BK>", /(\^|\bBK\b)/]], model: "<BK> TU UR 5NN 5NN 73 <BK>" },
          { they: `<BK> TU 73 E E`, done: true },
        ],
      },
    };
  }

  function viewQso() {
    const box = h("div.qso");
    const scen = h("div.tiles.small");
    const show = (key) => {
      const me = normalize(App.cfg.call || "");
      if (!me) { box.replaceChildren(needCall()); return; }
      const dx = makeDx();
      const sc = scenarios(me, dx)[key];
      runQso(sc, me, dx);
    };
    const list = scenarios("ME", makeDx());
    for (const k in list) {
      scen.appendChild(h("a.tile", { href: "javascript:void 0", onclick: () => show(k) },
        h("div", h("b", list[k].title), h("span", list[k].desc))));
    }

    function runQso(sc, me, dx) {
      token++;
      const t = token;
      const log = h("div.qlog");
      const st = stage();
      const typed = h("input.tr-answer", { type: "text", placeholder: "or type your reply here, Enter sends", autocomplete: "off", spellcheck: "false" });
      const ctl = { finish: null };
      const doneBtn = h("button.primary", { type: "button", onclick: () => ctl.finish && ctl.finish() }, "Done");
      const showT = h("input", { type: "checkbox", onchange: (e) => TPrefs.set("showQso", e.target.checked) });
      showT.checked = TPrefs.d.showQso;
      box.replaceChildren(h("h3", sc.title, h("small", " with " + dx.call)), log, st.el, typed,
        h("div.actions", doneBtn, h("button", { type: "button", onclick: () => { stopSession(); box.replaceChildren(); } }, "End"),
          h("label.check.small", showT, " show their text while it plays")));
      const line = (who, text, cls) => {
        log.appendChild(h("div.ql." + cls, h("small", who), h("span", dispText(text))));
        log.scrollTop = log.scrollHeight;
      };
      (async () => {
        for (const step of sc.steps) {
          if (!alive(t)) return;
          if (step.they) {
            st.set("listen", "", "", "Listen… " + dx.call + " is sending");
            const theirLine = h("div.ql.them" + (TPrefs.d.showQso ? "" : ".hide"), h("small", dx.call), h("span", dispText(step.they)));
            log.appendChild(theirLine);
            await playTrain(step.they);
            theirLine.classList.remove("hide");
            log.scrollTop = log.scrollHeight;
          }
          if (!alive(t)) return;
          if (step.done) {
            st.set("ok", "QSO complete", "", "Well done. Pick another scenario for a new station.");
            return;
          }
          let attempt = 0;
          for (;;) {
            st.set("turn", "", "", h("span", h("b", "Your turn: "), step.task));
            typed.value = "";
            let fromTyped = null;
            const onKey = (e) => { if (e.key === "Enter" && typed.value.trim()) { fromTyped = typed.value; ctl.finish && ctl.finish(); } };
            typed.addEventListener("keydown", onKey);
            const keyed = await captureKeyed({ idleMs: 3500, firstMs: 60000, maxMs: 120000, tok: t, stop: ctl,
              onChar: (g) => st.set("turn", undefined, undefined, h("span", h("b", "You: "), dispText(g))) });
            typed.removeEventListener("keydown", onKey);
            if (!alive(t)) return;
            const answer = normalize(fromTyped !== null ? fromTyped : keyed).replace(/\s+/g, " ").trim();
            line(me, answer || "(nothing)", "me");
            const results = step.checks.map(([label, re]) => [label, re.test(answer)]);
            const allOk = results.every((r) => r[1]);
            st.set(allOk ? "ok" : "bad", "", "", h("div.checks",
              ...results.map(([l, ok]) => h("span.ck" + (ok ? ".y" : ".n"), (ok ? "✓ " : "✗ ") + l)),
              h("div.model", h("small", "for example: "), dispText(step.model))));
            if (allOk) { await sleep(1600); break; }
            attempt++;
            const again = await new Promise((res) => {
              const retry = h("button.sm", { type: "button", onclick: () => res(true) }, "Try again");
              const skip = h("button.sm", { type: "button", onclick: () => res(false) }, "Continue anyway");
              st.el.querySelector(".tr-msg").appendChild(h("div.actions", retry, skip));
              const iv = setInterval(() => { if (!alive(t)) { clearInterval(iv); res(false); } }, 200);
            });
            if (!again) break;
          }
        }
      })();
    }

    return [
      h("p.sub", "SmartCW plays the other station, you answer on your paddles or key (or type). Your reply is checked for the parts a real QSO needs."),
      scen, box,
    ];
  }

  // ---------- 8. head copy ----------
  function viewHead() {
    const st = stage();
    const ta = h("textarea.copybox", { rows: 3, placeholder: "type what you copy, as you copy it", spellcheck: "false" });
    const sc = scoreChips();
    let overs = [], idx = 0, dx = null, scores = [];
    const newQso = () => { dx = makeDx(); overs = qsoText(normalize(App.cfg.call || "N0CALL"), dx); idx = 0; };
    newQso();
    const update = () => sc.set([["over", Math.min(idx + 1, overs.length) + "/" + overs.length],
      ["average", scores.length ? Math.round(scores.reduce((a, b) => a + b, 0) / scores.length) + "%" : "-"]]);
    st.set("", "", "", "A complete QSO, one over at a time. Type what you copy, then check.");
    let playing = false;
    const playBtn = h("button.primary", { type: "button", onclick: async () => {
      if (playing) return;
      playing = true;
      ta.value = "";
      ta.focus();
      st.set("listen", "", "", "Listen and type…");
      await playTrain(overs[idx]);
      playing = false;
      st.set("turn", undefined, undefined, "Finish typing, then Check");
    } }, icon("play"), "Play over");
    const check = h("button", { type: "button", onclick: () => {
      const want = normalize(overs[idx]).replace(/\s+/g, " ").trim();
      const got = normalize(ta.value).replace(/\s+/g, " ").trim();
      const pct = Math.max(0, Math.round(100 * (1 - levenshtein(want, got) / Math.max(1, want.length))));
      scores.push(pct);
      st.set(pct >= 90 ? "ok" : "bad", pct + "%", "", h("div.model", dispText(want)));
      idx++;
      if (idx >= overs.length) { toast("QSO finished: next one is a new station", "ok"); newQso(); }
      update();
    } }, "Check");
    update();
    return [st.el, ta, h("div.actions", playBtn, check,
      h("button", { type: "button", onclick: () => { newQso(); scores = []; update(); st.set("", "", "", "New station: " + "press Play over."); } }, "New QSO")), sc.el,
      h("p.hint", "Score = how close your copy is, character by character. Try writing only what you are sure of.")];
  }

  // ---------- 9. fist analyzer ----------
  function viewFist() {
    let els = [];
    const chart = h("div.fchart");
    const sc = scoreChips();
    const tips = h("ul.tips");
    const draw = () => {
      if (!els.length) { chart.replaceChildren("Start keying: every mark and space shows up here."); sc.set([["speed", "-"], ["dit", "-"], ["dah", "-"], ["ratio", "-"]]); tips.replaceChildren(); return; }
      const last = els.slice(-48);
      const dits = last.filter((e) => e.v === ".").map((e) => e.m);
      const dahs = last.filter((e) => e.v === "-").map((e) => e.m);
      const avg = (a) => (a.length ? a.reduce((x, y) => x + y, 0) / a.length : 0);
      const sd = (a) => { const m = avg(a); return a.length > 1 ? Math.sqrt(avg(a.map((x) => (x - m) ** 2))) : 0; };
      const dit = avg(dits) || (dahs.length ? avg(dahs) / 3 : 0);
      const gapsEl = last.filter((e) => e.g && dit && e.g < 2 * dit).map((e) => e.g);
      const gapsCh = last.filter((e) => e.g && dit && e.g >= 2 * dit && e.g < 5 * dit).map((e) => e.g);
      const ratio = dit && dahs.length ? avg(dahs) / dit : 0;
      sc.set([
        ["speed", dit ? Math.round(1200 / dit) + " wpm" : "-"],
        ["dit", dits.length ? Math.round(avg(dits)) + " ms ±" + Math.round((100 * sd(dits)) / avg(dits)) + "%" : "-"],
        ["dah", dahs.length ? Math.round(avg(dahs)) + " ms ±" + Math.round((100 * sd(dahs)) / avg(dahs)) + "%" : "-"],
        ["ratio", ratio ? ratio.toFixed(2) : "-", ratio && (ratio < 2.6 || ratio > 3.6) ? "warn" : ""],
        ["element gap", gapsEl.length ? (avg(gapsEl) / dit).toFixed(1) + " dit" : "-"],
        ["letter gap", gapsCh.length ? (avg(gapsCh) / dit).toFixed(1) + " dit" : "-"],
      ]);
      const t = [];
      if (ratio && ratio < 2.6) t.push("Your dahs are short: aim for three dits long.");
      if (ratio > 3.6) t.push("Your dahs are long: aim for three dits long.");
      if (dits.length > 4 && sd(dits) / avg(dits) > 0.25) t.push("Your dits vary a lot in length: try to keep a steady rhythm.");
      if (gapsEl.length > 4 && avg(gapsEl) / dit > 1.5) t.push("The space inside letters is long, letters may break up.");
      if (gapsCh.length > 2 && avg(gapsCh) / dit < 2.6) t.push("Letters run together: leave three dits between them.");
      if (!t.length && last.length > 10) t.push("Nice and even. Keep going.");
      if (App.cfg.keyer !== "straight" && App.cfg.keyer !== "bug") t.push("Iambic keyers make perfect marks; with them only your spacing is measured. A straight key or bug shows your real fist.");
      tips.replaceChildren(...t.map((x) => h("li", x)));
      // timeline
      const W = 900, H = 70;
      const total = last.reduce((s, e) => s + Math.min(e.g || 0, 6 * (dit || 60)) + e.m, 0) || 1;
      const sx = W / total;
      let x = 0;
      const NS = "http://www.w3.org/2000/svg";
      const svg = document.createElementNS(NS, "svg");
      svg.setAttribute("viewBox", `0 0 ${W} ${H}`);
      svg.setAttribute("class", "ftime");
      for (const e of last) {
        x += Math.min(e.g || 0, 6 * (dit || 60)) * sx;
        const r = document.createElementNS(NS, "rect");
        r.setAttribute("x", x.toFixed(1));
        r.setAttribute("y", e.v === "." ? 22 : 14);
        r.setAttribute("width", Math.max(1, e.m * sx).toFixed(1));
        r.setAttribute("height", e.v === "." ? 26 : 42);
        r.setAttribute("rx", 3);
        r.setAttribute("class", e.v === "." ? "fd" : "fh");
        svg.appendChild(r);
        x += e.m * sx;
      }
      chart.replaceChildren(svg);
    };
    const onEl = (m) => { els.push({ v: m.v, m: m.m, g: m.g > 3000 ? 0 : m.g }); if (els.length > 200) els = els.slice(-200); draw(); };
    Bus.on("el", onEl);
    draw();
    return {
      els: [h("p.sub", "Key anything: SmartCW measures every mark and space you make and shows how even your fist is."),
        chart, sc.el, tips, h("div.actions", h("button", { type: "button", onclick: () => { els = []; draw(); } }, "Reset"))],
      cleanup: () => Bus.off("el", onEl),
    };
  }

  // ---------- 10. progress ----------
  function viewStats() {
    const grid = h("div.cgrid");
    const render2 = () => {
      grid.replaceChildren();
      KOCH.forEach((c, i) => {
        const r = Stats.recent(c);
        const s = Stats.d.c[c];
        const cls = r === null ? "none" : r >= 0.9 ? "good" : r >= 0.7 ? "mid" : "low";
        grid.appendChild(h("div.cs." + cls + (i < Stats.d.koch ? ".in" : ""), { title: s ? `${s.ok}/${s.n} correct overall` : "not practised yet" },
          h("b", disp(c)), h("small", r === null ? "-" : Math.round(r * 100) + "%")));
      });
    };
    render2();
    const fileIn = h("input", { type: "file", accept: ".json,application/json", hidden: true, onchange: async (e) => {
      const f = e.target.files[0];
      if (!f) return;
      try { Stats.d = Object.assign(Stats.load(), JSON.parse(await f.text())); Stats.save(); render2(); toast("Progress imported", "ok"); } catch (err) { toast("Not a SmartCW progress file", "err"); }
    } });
    const best = Object.entries(Stats.d.best).map(([k, v]) => h("span.chip", h("small", k === "mycall" ? "own call" : k), " ", h("b", v + " wpm")));
    return [
      h("p.sub", "Recent accuracy per character, in Koch order. Highlighted characters are in your current Koch set. Progress lives in this browser."),
      h("div.tr-score", h("span.chip", h("small", "Koch level"), " ", h("b", String(Stats.d.koch))),
        h("span.chip", h("small", "answers"), " ", h("b", String(Stats.d.total))), ...best),
      grid,
      h("div.actions",
        h("button", { type: "button", onclick: () => { Stats.d.koch = Math.max(2, Stats.d.koch - 1); Stats.save(); render2(); } }, "Level −"),
        h("button", { type: "button", onclick: () => { Stats.d.koch = Math.min(KOCH.length, Stats.d.koch + 1); Stats.save(); render2(); } }, "Level +"),
        h("button", { type: "button", onclick: () => {
          const a = h("a", { href: URL.createObjectURL(new Blob([JSON.stringify(Stats.d)], { type: "application/json" })), download: "smartcw-progress.json" });
          document.body.appendChild(a); a.click(); a.remove();
        } }, icon("download"), "Export"),
        h("button", { type: "button", onclick: () => fileIn.click() }, icon("upload"), "Import"), fileIn,
        h("button", { type: "button", onclick: () => { if (confirm("Forget all trainer progress in this browser?")) { localStorage.removeItem("smartcw.stats"); Stats.load(); render2(); } } }, "Reset")),
    ];
  }

  // ---------- page ----------
  const VIEWS = {
    echo: ["Listen and repeat", "key", "SmartCW plays, you key it back on the paddles. Koch method, iambic keyer.", viewEcho],
    copy: ["Copy characters", "ear", "Koch groups to copy by ear and type.", viewCopy],
    mycall: ["Your callsign", "user", "Hear your own call faster and faster, or key it cleanly.", viewMyCall],
    calls: ["Callsigns", "id", "Random calls from real prefixes, speed goes up as you get them.", viewCalls],
    words: ["Words and abbreviations", "book", "Common words, ham abbreviations and Q-codes as whole words.", viewWords],
    codes: ["Q-codes and prosigns", "chat", "What QRL, QSB, <KN> and friends mean, with a reference.", viewCodes],
    qso: ["QSO practice", "radio", "Answer a CQ, call CQ, a contest and a park activation.", viewQso],
    head: ["Head copy", "pen", "Copy a complete QSO, over by over, and get a score.", viewHead],
    fist: ["Fist analyzer", "wave", "How even are your dits, dahs and spaces?", viewFist],
    stats: ["Progress", "chart", "Accuracy per character and your Koch level.", viewStats],
  };
  let cleanup = null;

  function render(sub) {
    if (cleanup) { cleanup(); cleanup = null; }
    view = VIEWS[sub] ? sub : null;
    const sec = $("#p-train");
    sec.replaceChildren();
    if (!view) {
      sec.append(h("div.page-head", h("h2", icon("school"), "Trainer"),
        h("p.sub", "Learn and sharpen CW with SmartCW's own sidetone and your own paddles. Nothing is transmitted while you are on these pages.")),
      speedBar(),
      h("div.tiles", ...Object.entries(VIEWS).map(([k, v]) => h("a.tile", { href: "#/train/" + k },
        h("div.dp-ico", icon(v[1])), h("div", h("b", v[0]), h("span", v[2]))))),
      h("a.tile.wide", { href: "#/live" }, h("div.dp-ico", icon("tree")), h("div", h("b", "Morse tree"),
        h("span", "On the Live page: tap the key to walk the tree, or tap a letter to hear it."))));
      return;
    }
    const [title, ic, desc, fn] = VIEWS[view];
    const out = fn();
    let els = out;
    if (out && out.els) { els = out.els; cleanup = out.cleanup; }
    sec.append(h("div.page-head", h("a.back", { href: "#/train" }, icon("chev", "left"), "Trainer"), h("h2", icon(ic), title), h("p.sub", desc)),
      view === "fist" || view === "stats" ? null : speedBar(), h("div.card.trainer", ...els));
  }

  return {
    show(sub) {
      if (!Stats.d) Stats.load();
      if (!TPrefs.d) TPrefs.load();
      if (!held) { Practice.hold(); held = true; }
      stopSession();
      render(sub);
    },
    leave() {
      stopSession();
      if (cleanup) { cleanup(); cleanup = null; }
      if (held) { Practice.release(); held = false; }
    },
  };
})();
