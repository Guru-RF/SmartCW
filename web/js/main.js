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
  Bus.on("state", () => {
    call.textContent = (App.cfg.call ? App.cfg.call + " · " : "") + "CW keyer & trainer";
    banner.replaceChildren();
    if (App.rt.restart) {
      banner.appendChild(h("div.warn.bar-warn", "USB or network settings changed: they apply after a restart. ",
        h("button.sm", { type: "button", onclick: async () => { await post("/api/restart"); toast("Restarting…"); } }, "Restart now")));
    }
    if (App.rt.drive) banner.appendChild(h("div.warn.bar-warn", "The SMARTCW drive is open on your computer: settings changed here are not saved, the config.txt on the drive wins when it is ejected."));
    $("#ftr-ver").textContent = "SmartCW " + App.v;
  });

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
  window.addEventListener("hashchange", () => Router.route());
  Router.route();
  setInterval(() => { if (!App.connected) refreshState(); }, 5000);
})();

function sleepMs(ms) { return new Promise((r) => setTimeout(r, ms)); }
