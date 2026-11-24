/* Start-up: settings metadata, state, live events, header status, routing. */
"use strict";

(async function main() {
  const conn = $("#conn");
  const air = $("#hdr-air");
  const banner = $("#banner");
  const call = $("#brand-call");

  Bus.on("conn", (on) => {
    conn.classList.toggle("error", !on);
    conn.lastChild.textContent = on ? "connected" : "reconnecting…";
  });
  Bus.on("k", () => air.classList.toggle("on", App.txDown));
  Bus.on("ptt", (m) => air.classList.toggle("ptt", !!m.d));
  function renderBanner() {
    call.textContent = (App.cfg.call ? App.cfg.call + " · " : "") + "CW keyer & trainer";
    banner.replaceChildren();
    if (App.rt.restart) {
      banner.appendChild(h("div.warn.bar-warn", "USB or network settings changed: they apply after a restart. ",
        h("button.sm", { type: "button", onclick: async () => { await post("/api/restart"); toast("Restarting…"); } }, "Restart now")));
    }
    if (App.rt.wk) banner.appendChild(h("div.warn.note.bar-warn", "A contest logger has SmartCW open as a WinKeyer: its speed, weight and tone apply until it closes, then yours come back."));
    if (App.rt.drive) banner.appendChild(h("div.warn.bar-warn", "The SMARTCW drive is open on your computer: settings changed here are not saved, the config.txt on the drive wins when it is ejected."));
    if (App.latest && verNewer(App.latest, App.v) && Update.skipped() !== App.latest) {
      banner.appendChild(h("div.warn.note.bar-warn", "Firmware " + App.latest + " is out; this SmartCW runs " + App.v + ". ",
        h("a", { href: App.latestUrl, target: "_blank", rel: "noopener" }, "Update at smartcw.app"), " ",
        h("button.sm", { type: "button", onclick: () => { Update.skip(App.latest); renderBanner(); } }, "Not now")));
    }
    $("#ftr-ver").textContent = "SmartCW " + App.v;
  }
  Bus.on("state", renderBanner);

  for (let i = 0; ; i++) {
    try {
      App.meta = await getJSON("/api/meta");
      for (const m of App.meta) App.metaBy[m.k] = m;
      applyState(await getJSON("/api/state"));
      break;
    } catch (e) {
      conn.classList.add("error");
      conn.lastChild.textContent = "waiting for SmartCW…";
      await sleepMs(Math.min(5000, 800 + i * 400));
    }
  }
  Live.start();
  Update.check().then((found) => { if (found) renderBanner(); });
  window.addEventListener("hashchange", () => Router.route());
  Router.route();
  setInterval(() => { if (!App.connected) refreshState(); }, 5000);
})();

function sleepMs(ms) { return new Promise((r) => setTimeout(r, ms)); }

/* Is a newer than b? Versions are "2.4.1". */
function verNewer(a, b) {
  const pa = String(a).split(".").map(Number), pb = String(b).split(".").map(Number);
  for (let i = 0; i < 3; i++) {
    const x = pa[i] || 0, y = pb[i] || 0;
    if (x !== y) return x > y;
  }
  return false;
}

/* The update notice, asked for by Dave, G3LRC. SmartCW cannot update itself:
   its only network is the USB cable, and a browser writes firmware only after
   a click. What this page can do is ask smartcw.app, from the browser, whether
   a newer release exists, once a day, and say so at the top and on System.
   Off with the updateCheck setting. */
const Update = {
  KEY: "smartcw.update",
  URL: "https://smartcw.app/api/firmware",
  load() { try { return JSON.parse(localStorage.getItem(this.KEY) || "{}"); } catch (e) { return {}; } },
  save(st) { try { localStorage.setItem(this.KEY, JSON.stringify(st)); } catch (e) { /* private mode */ } },
  skipped() { return this.load().skip; },
  skip(v) { const st = this.load(); st.skip = v; this.save(st); },
  async check() {
    if (App.cfg.updateCheck === false) return false;
    let st = this.load();
    if (!st.v || !st.t || Date.now() - st.t > 86400e3 || st.t > Date.now()) {
      try {
        const r = await fetch(this.URL, { cache: "no-store" });
        if (!r.ok) return false;
        const j = await r.json();
        if (typeof j.version !== "string") return false;
        st = { t: Date.now(), v: j.version, url: typeof j.url === "string" ? j.url : "https://smartcw.app/update", skip: st.skip };
        this.save(st);
      } catch (e) {
        return false; // no internet, or the site is down: say nothing
      }
    }
    App.latest = st.v;
    App.latestUrl = st.url || "https://smartcw.app/update";
    return true;
  },
};
