/* Settings page: every setting, applied immediately and saved by SmartCW. */
"use strict";

// Two columns that line up. The first cards of each side form one block whose
// bottoms meet (the shorter side's last card stretches); the remaining cards
// sit side by side in rows, so each pair has the same height.
function pairLayout(headL, headR, left, right) {
  const l = [...left.children], r = [...right.children];
  const grid = h("div.pgrid", h("div.pcol", l.slice(0, headL)), h("div.pcol", r.slice(0, headR)));
  const restL = l.slice(headL), restR = r.slice(headR);
  for (let i = 0; i < Math.max(restL.length, restR.length); i++)
    grid.append(restL[i] || h("div"), restR[i] || h("div"));
  return grid;
}

Pages.settings = (() => {
  let built = false;
  let savedChip;

  function build() {
    const sec = $("#p-settings");
    savedChip = h("span.status", h("span.dot"), "Saved");
    const restartNote = h("small.rnote", " restart");
    const rn = () => restartNote.cloneNode(true);

    const memRows = [];
    for (let i = 1; i <= 10; i++) {
      memRows.push(h("div.memrow", h("b", "M" + i), textInput("mem" + i, { maxlength: 71, placeholder: "empty" }),
        h("button.sm", { type: "button", "aria-label": "play memory " + i, onclick: () => post("/api/mem", { n: i, p: "practice" }) }, icon("play"))));
    }

    sec.append(
      h("div.page-head", h("h2", icon("sliders"), "Settings"),
        h("p.sub", "Changes apply at once and SmartCW saves them to config.txt a moment later. Settings marked restart change the USB layout and take effect after a restart.")),
      h("div.actions.top", savedChip,
        h("button", { type: "button", onclick: resetDefaults }, icon("sync"), "Reset to defaults")),
      pairLayout(2, 3,
        h("div",
          card("key", "Keyer", "Key type, speed and the feel of the paddles",
            row("Key type", "a long press on the PTT button toggles straight",
              segmented("keyer", ["iambicA", "iambicB", "ultimatic", "bug", "straight"], ["Iambic A", "Iambic B", "Ultimatic", "Bug", "Straight"]), true),
            row("Speed", "words per minute (PARIS)", rangeInput("wpm", " wpm")),
            row("Weight", "50 % is standard, more makes marks heavier", rangeInput("weight", " %")),
            row("Dah ratio", "dah length in dits, 3.0 is standard", rangeInput("ratio", "", (v) => (v / 10).toFixed(1))),
            row("Swap paddles", "for left-handed operators", switchInput("paddleSwap")),
            row("Autospace", "iambic: a pause always becomes a full letter space", switchInput("autoSpace"))),
          card("speaker", "Sidetone", "Buzzer and line out",
            row("Tone", "sidetone pitch", h("div.inl", rangeInput("tone", " Hz", null, 10),
              h("button.sm", { type: "button", onclick: () => post("/api/beep", { n: 1 }) }, "Test"))),
            row("Buzzer", "the piezo in SmartCW", switchInput("buzzer")),
            row("Buzzer level", "1 to 10", rangeInput("volume")),
            row("Line out level", "1 to 10, for headphones or the radio's input", rangeInput("lineLevel"))),
          card("antenna", "Transmit output", "What the output jack does with transmitted CW",
            row("Output", "", segmented("output", ["key", "audio", "off"], ["Key line", "Audio + PTT", "Practice"])),
            h("p.hint", "Key line: the jack keys your radio like a straight key. Audio + PTT: the jack is PTT and the CW tone goes out on line out, for FM, SSB or a data input. Practice: nothing is transmitted."),
            row("PTT lead", "audio mode: time between PTT and the first tone", rangeInput("pttLead", " ms", null, 10)),
            row("PTT tail", "audio mode: PTT stays on this long after the last tone", rangeInput("pttTail", " ms", null, 50)),
            row("Key compensation", "added to every transmitted dit and dah, for QSK radios that shorten them", rangeInput("keyComp", " ms")),
            row("First element", "added to the first dit or dah after a pause, for radios that clip it", rangeInput("firstExt", " ms", null, 5))),
          card("cw", "Text sending", "Text from the browser, TCP, serial and memories",
            row("Farnsworth", "extra space between letters at this effective speed; off = normal spacing",
              rangeInput("farnsworth", "", (v) => (v ? v + " wpm" : "off")))),
          card("school", "Trainer", "Speed and pitch for the trainer pages",
            row("Character speed", "how fast each letter sounds", rangeInput("trainWpm", " wpm")),
            row("Effective speed", "Farnsworth spacing for the trainer; off = normal spacing", rangeInput("trainEff", "", (v) => (v ? v + " wpm" : "off"))),
            row("Trainer tone", "0 = the same as the sidetone", rangeInput("trainTone", "", (v) => (v ? v + " Hz" : "same"), 10)))),
        h("div",
          card("user", "Station", "Used in memories as {call} and {name}, and by the trainer",
            row("Callsign", "", textInput("call", { maxlength: 15, placeholder: "ON6URE", style: { textTransform: "uppercase" } })),
            row("Name", "", textInput("name", { maxlength: 19, placeholder: "your name" }))),
          card("list", "Memories", "Send them from the Live page, TCP or serial (!mem 1). {call}, {name}, {nr}, <SK> <KN> <AR> <BT> work.",
            ...memRows,
            row("Serial number", "{nr} sends it, then moves on for the next contact; {lastnr} repeats it", numInput("nr")),
            row("Cut numbers", "in serial numbers", segmented("cutNumbers", ["off", "t", "tn"], ["Off", "0 → T", "0 → T, 9 → N"]))),
          card("beacon", "Beacon", "Repeats a message, e.g. for antenna tests or fox hunts",
            row("Message", "", textInput("beaconText", { maxlength: 71 })),
            row("Every", "seconds between transmissions", rangeInput("beaconEvery", " s", null, 5))),
          card("music", "USB MIDI", "For SDR software that keys CW from MIDI notes",
            row(["MIDI port", rn()], "", switchInput("midi")),
            row("MIDI mode", "keyed: the keyer's output as one note; paddles: the raw paddle contacts, the SDR keys",
              segmented("midiMode", ["keyed", "paddles"], ["Keyed", "Paddles"])),
            h("div.notes",
              h("label", "CW note", numInput("midiCwNote")),
              h("label", "Straight key", numInput("midiStraightNote")),
              h("label", "PTT", numInput("midiPttNote")),
              h("label", "Dit contact", numInput("midiDitNote")),
              h("label", "Dah contact", numInput("midiDahNote"))),
            h("div.actions.tight",
              h("button.sm", { type: "button", onclick: () => midiNotes([0, 0, 1, 2]) }, "Vail notes"),
              h("button.sm", { type: "button", onclick: () => midiNotes(null) }, "SmartCW notes"),
              h("small.dim", "Vail: keyed = straight key note 0, paddles = notes 1 and 2"))),
          card("keyboard", "Computer", "Keyboard, internet CW and contest loggers",
            row(["USB keyboard", rn()], "SmartCW is also a keyboard", switchInput("keyboard")),
            row("Keyboard sends", "typing: decoded CW as letters, PTT = Enter; paddle keys: Left and Right Ctrl, for vBand and Vail",
              segmented("keyboardMode", ["text", "ctrl"], ["Typing", "Paddle keys"])),
            row("WinKeyer", "contest loggers (N1MM+, DXLog, Win-Test, fldigi, CQRLOG) see a WinKeyer on the serial port", switchInput("winkeyer"))),
          card("network", "USB network", "This web page and the TCP port, over the USB cable",
            row(["USB Ethernet", rn()], "turning it off removes this web interface", switchInput("network")),
            row(["Address", rn()], "your computer gets an address next to it automatically", textInput("ip", { maxlength: 15, inputmode: "decimal" })),
            row(["TCP port", rn()], "text and command protocol, like the serial port", numInput("tcpPort")),
            row(["cwdaemon port", rn()], "UDP, for Linux loggers (CQRLOG, TLF, xlog); 0 = off", numInput("cwdaemonPort"))))));

    Bus.on("saved", () => flashSaved());
    Bus.on("state", () => {
      savedChip.classList.toggle("form-unsaved", !!App.rt.dirty);
      savedChip.lastChild.textContent = App.rt.dirty ? "Saving…" : "Saved";
    });
  }

  // MIDI note numbers: cw, straight, dit, dah; null = SmartCW's defaults.
  function midiNotes(v) {
    ["midiCwNote", "midiStraightNote", "midiDitNote", "midiDahNote"].forEach((k, i) =>
      Settings.set(k, v ? v[i] : App.metaBy[k].d));
  }

  function flashSaved() {
    savedChip.classList.add("pulse");
    setTimeout(() => savedChip.classList.remove("pulse"), 600);
  }

  async function resetDefaults() {
    if (!confirm("Reset all settings to their defaults? Your callsign, name, memories and beacon text are kept.")) return;
    const r = await post("/api/defaults", {});
    if (r && r.cfg) { Settings.pending = {}; applyState(r); toast("Defaults restored", "ok"); }
  }

  return { show() { if (!built) { build(); built = true; } } };
})();
