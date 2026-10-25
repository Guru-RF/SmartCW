/* System page: status, console, backup, firmware update, integration help. */
"use strict";

Pages.system = (() => {
  let built = false;
  let statusRows;

  function statusCard() {
    statusRows = h("div");
    return card("cpu", "Status", "SmartCW right now", statusRows);
  }

  function renderStatus() {
    if (!statusRows) return;
    const rt = App.rt, c = App.cfg;
    const yes = (b, extra) => h("span.pill" + (b ? ".up" : ".off"), b ? "on" : "off", extra ? " " + extra : "");
    const r = (k, v) => h("div.kv", h("span.k", k), h("span.v", v));
    statusRows.replaceChildren(...[
      r("Firmware", "SmartCW " + App.v),
      r("Serial number", h("span", App.id || "not set", h("small.dim", "  ·  chip " + App.chip))),
      r("Uptime", fmtUptime(App.up)),
      r("Key type", c.keyer || "-"),
      r("Output", c.output + (rt.practice ? " (practice held by a trainer)" : "")),
      r("USB serial", c.winkeyer ? h("span", yes(true), rt.wk ? "  ·  WinKeyer open by a logger" : "  ·  WinKeyer ready") : yes(true)),
      r("USB MIDI", yes(rt.midi, rt.midi ? "(" + c.midiMode + ")" : "")),
      r("USB keyboard", yes(rt.kbd, rt.kbd ? "(" + (c.keyboardMode === "ctrl" ? "paddle keys" : "typing") + ")" : "")),
      r("USB drive", yes(rt.drive)),
      r("Web", h("span", "http://" + rt.ip + "/  ·  http://smartcw.local/  ·  " + rt.web + " open")),
      r("TCP port", h("span", String(c.tcpPort) + "  ·  " + rt.tcp + " connected")),
      r("cwdaemon", c.cwdaemonPort ? "UDP port " + c.cwdaemonPort : yes(false)),
      r("Settings", rt.dirty ? "saving…" : "saved to config.txt"),
      rt.restart ? h("div.warn", "Some changes need a restart. ", h("button.sm", { type: "button", onclick: restart }, "Restart now")) : null,
    ].filter(Boolean));
  }

  function consoleCard() {
    const out = h("pre.console");
    const inp = h("input", { type: "text", placeholder: "!help", spellcheck: "false", autocomplete: "off" });
    const hist = [];
    let hi = 0;
    const run = async (line) => {
      line = line.trim();
      if (!line) return;
      hist.push(line);
      hi = hist.length;
      out.textContent += "> " + line + "\n";
      if (!line.startsWith("!")) {
        await post("/api/send", { text: line });
        out.textContent += "(sent as CW)\n";
      } else {
        const t = await post("/api/cmd", { line });
        out.textContent += (typeof t === "string" ? t : JSON.stringify(t)) + "\n";
      }
      out.scrollTop = out.scrollHeight;
      refreshState();
    };
    inp.addEventListener("keydown", (e) => {
      if (e.key === "Enter") { run(inp.value); inp.value = ""; }
      else if (e.key === "ArrowUp" && hi > 0) { inp.value = hist[--hi]; e.preventDefault(); }
      else if (e.key === "ArrowDown") { hi = Math.min(hist.length, hi + 1); inp.value = hist[hi] || ""; e.preventDefault(); }
    });
    const quick = (cmd) => h("button.sm", { type: "button", onclick: () => run(cmd) }, cmd);
    return card("terminal", "Console", "The same commands as the serial port and the TCP port. Text without ! is sent as CW.",
      out, h("div.cline", h("b", ">"), inp),
      h("div.actions.tight", quick("!help"), quick("!status"), quick("!get"), quick("!net"),
        h("button.sm", { type: "button", onclick: () => (out.textContent = ""), style: { marginLeft: "auto" } }, "Clear")));
  }

  function backupCard() {
    const file = h("input", { type: "file", accept: ".txt,text/plain", hidden: true, onchange: async (e) => {
      const f = e.target.files[0];
      if (!f) return;
      const text = await f.text();
      const r = await fetch("/config.txt", { method: "POST", body: text });
      if (r.ok) { applyState(await r.json()); toast("Settings restored from " + f.name, "ok"); }
      else toast("Restore failed", "err");
      file.value = "";
    } });
    return card("archive", "Backup", "All settings in one readable file, config.txt",
      h("p", "Download a copy before experimenting, restore it later or on another SmartCW."),
      h("div.actions",
        h("a.btn", { href: "/config.txt", download: "smartcw-config.txt" }, icon("download"), "Download config.txt"),
        h("button", { type: "button", onclick: () => file.click() }, icon("upload"), "Restore from file"), file),
      h("p.hint", "The same file is on the SMARTCW drive: hold both paddles while plugging in, or send !drive."));
  }

  function updateCard() {
    return card("download", "Firmware update", "New firmware is a .uf2 file",
      h("ol.howto",
        h("li", "Download the latest smartcw-*.uf2 from ", h("a", { href: "https://github.com/Guru-RF/SmartCW/releases", target: "_blank", rel: "noopener" }, "github.com/Guru-RF/SmartCW"), "."),
        h("li", "Press the button below: SmartCW restarts as a drive called RPI-RP2."),
        h("li", "Copy the .uf2 file onto that drive. SmartCW restarts with the new firmware, settings are kept.")),
      h("div.actions", h("button.primary", { type: "button", onclick: bootloader }, icon("download"), "Restart into update mode")),
      h("p.hint", "Without this page: send !bootloader over serial or TCP, or put the single word firmwareupdate in config.txt."));
  }

  function maintCard() {
    return card("sync", "Maintenance", "",
      row("Restart", "applies USB and network changes", h("button", { type: "button", onclick: restart }, "Restart")),
      row("Factory reset", "everything back to defaults, including callsign, memories and beacon text",
        h("button.danger", { type: "button", onclick: factory }, "Factory reset")));
  }

  function helpCard() {
    const c = App.cfg;
    const t = h("div.help");
    const render = () => {
      t.replaceChildren(
        h("h4", "SDR software over MIDI"),
        h("p", "Pick the MIDI port called SmartCW, channel 1. In keyed mode SmartCW is the keyer: map the notes below to CW key and PTT. In paddles mode the SDR keys: map the dit and dah notes to its paddle inputs."),
        h("table.mini", h("tbody",
          h("tr", h("td", "CW key (paddles and text)"), h("td", h("code", "note " + App.cfg.midiCwNote))),
          h("tr", h("td", "Straight key"), h("td", h("code", "note " + App.cfg.midiStraightNote))),
          h("tr", h("td", "PTT button, audio-mode PTT"), h("td", h("code", "note " + App.cfg.midiPttNote))),
          h("tr", h("td", "Dit / dah contact (paddles mode)"), h("td", h("code", "notes " + App.cfg.midiDitNote + " / " + App.cfg.midiDahNote))))),
        h("h4", "Contest loggers"),
        h("p", "WinKeyer: in N1MM+, DXLog, Win-Test, fldigi, CQRLOG or any WinKeyer logger, pick SmartCW's serial port as a WinKeyer (WK2). " +
          "cwdaemon: point CQRLOG, TLF or xlog at " + (App.rt.ip || "192.168.7.1") + " port " + (c.cwdaemonPort || 6789) + "."),
        h("h4", "Internet CW (vBand, Vail)"),
        h("p", "Settings → Computer: USB keyboard on, keyboard sends Paddle keys. The paddles become Left and Right Ctrl. Or use MIDI with the Vail notes."),
        h("h4", "TCP and serial"),
        h("p", "Characters you send are keyed, decoded paddle input comes back as text. Lines starting with ! are commands."),
        h("pre.code", `nc ${App.rt.ip || "192.168.7.1"} ${c.tcpPort}\n!wpm 22\ncq cq de ${(c.call || "on6ure").toLowerCase()} k`),
        h("h4", "Web API"),
        h("p", "GET /api/state, POST /api/set (wpm=22&tone=650), POST /api/send (text=...), GET /api/events (live events). See INTEROP.md."));
    };
    Bus.on("state", render);
    render();
    return card("network", "Connect your software", "MIDI, loggers, internet CW, TCP and the API", t);
  }

  async function restart() {
    if (!confirm("Restart SmartCW? The page reconnects by itself.")) return;
    await post("/api/restart");
    toast("Restarting…");
  }
  async function bootloader() {
    if (!confirm("Restart into firmware update mode? SmartCW disappears until a .uf2 is copied or it is replugged.")) return;
    await post("/api/bootloader");
    toast("SmartCW is now the RPI-RP2 drive");
  }
  async function factory() {
    if (!confirm("Factory reset: all settings, your callsign and memories are cleared. Continue?")) return;
    const r = await post("/api/defaults", { scope: "all" });
    if (r && r.cfg) { Settings.pending = {}; applyState(r); toast("Factory settings restored", "ok"); }
  }

  function build() {
    $("#p-system").append(
      h("div.page-head", h("h2", icon("cpu"), "System"), h("p.sub", "Status, the command console, backups and firmware updates.")),
      h("div.cols", h("div", statusCard(), consoleCard(), helpCard()), h("div", backupCard(), updateCard(), maintCard())));
    Bus.on("state", renderStatus);
    renderStatus();
  }

  return { show() { if (!built) { build(); built = true; } renderStatus(); } };
})();
