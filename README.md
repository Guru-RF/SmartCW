# SmartCW – Iambic Keyer, CW Trainer and CW Interface

SmartCW is a USB-C iambic keyer, CW trainer and CW interface for radios and SDR software, with its own sidetone. Plug in paddles or a straight key, plug in USB, and SmartCW keys your transceiver, your SDR software (over MIDI) or types what you key as text, while sending anything you type as Morse.

Over the same USB-C cable it is also a small network adapter with a **web interface** in the RF.Guru style: the visual Morse tree, every setting, operating modes and a complete CW trainer, at **http://smartcw.local/** (or http://192.168.7.1/). No drivers, no app, no internet connection needed.

The firmware is C++ for the RP2040 (Arduino core by earlephilhower, built with PlatformIO), structured like [SmartFOX](https://github.com/Guru-RF/SmartFOX). All settings live in a plain `config.txt`.

Buy here: [SmartCW – RF.Guru shop](https://shop.rf.guru)

![Live page](docs/screenshots/web_live.png)

---

## 🧠 What it does

- **Keyer**: iambic A, iambic B, ultimatic, bug or straight key on the paddle jack, with weighting, dah ratio, paddle swap and autospace. A long press on PTT switches to straight key and back.
- **Transmit output**, three ways: the jack **keys the radio** like a straight key, or it is **PTT with the CW tone on line out** (audio mode, for FM, SSB or a data input, with PTT lead and tail), or **practice** (nothing transmitted).
- **Sidetone** on the piezo and the line-out jack, pitch and level adjustable.
- **Decodes what you key** (adaptive for straight key) and sends it to the computer over USB serial, TCP and the web page.
- **Sends text as CW**: from the web page, serial, TCP or the six memories, with Farnsworth spacing and prosigns like `<SK>`, `<KN>`, `<AR>`, `<BT>`.
- **USB MIDI** for SDR software: the keyer's output as one note, or the raw paddle contacts as two notes; a PTT note.
- **USB keyboard**: decoded CW is typed into whatever window has focus, PTT = Enter.
- **Beacon**: repeats a message every N seconds; a paddle press stops it.
- **Tune**: key down for up to 20 seconds.
- **Web interface and TCP port** over USB Ethernet (see below), and a USB drive mode for editing `config.txt`.

The paddles always win: pressing a paddle aborts any text, beacon or tune in progress.

MIDI, keyboard mode, the serial/TCP protocol and the web API are described in [INTEROP.md](INTEROP.md).

---

## 🌐 Web interface

Plug SmartCW in and open **http://smartcw.local/** or **http://192.168.7.1/**. SmartCW gives your computer an address next to it by itself (192.168.7.16 and up); it hands out no gateway and no DNS server, so your normal internet connection is not touched. Linux, macOS and Windows 10/11 support the USB network natively.

`smartcw.local` is answered by SmartCW's own mDNS responder for IPv4 and for its IPv6 link-local address, so it resolves at once through Avahi (Linux), Bonjour (macOS) and Windows. SmartCW also announces itself with DNS-SD: service browsers (`avahi-browse -a`, Bonjour browsers, file managers) list **SmartCW** as a *Web Site* and as a *Telnet* service for the TCP port.

| Page | What is on it |
| --- | --- |
| **Live** | The visual Morse tree (follows every dit and dah from your paddles, tap a letter to hear it, swipe for numbers and signs), the decoded text with its Morse, an on-screen key (tap = dot, hold = dash), key type and output mode, speed and tone, text sending, memories, beacon, tune, PTT and a monitor of everything sent and received |
| **Trainer** | Listen and repeat (Koch method, iambic keyer), copy characters, your own callsign, callsigns, words and abbreviations, Q-codes and prosigns, QSO practice (answer a CQ, call CQ, contest, park hunter), head copy of a complete QSO, a fist analyzer and progress per character |
| **Settings** | Every setting, applied at once and saved to `config.txt` a moment later, with a **Reset to defaults** button that keeps your callsign, name and memories |
| **System** | Status, a command console, backup and restore of `config.txt`, firmware update and the MIDI/TCP reference |

The trainer pages hold **practice mode** while they are open: SmartCW never transmits during training, and transmits normally again within ten seconds after you leave. Trainer speed (character and effective wpm) and pitch are separate from the keyer settings. Progress is kept in the browser and can be exported.

| Trainer | Settings |
| --- | --- |
| ![Trainer](docs/screenshots/web_trainer.png) | ![Settings](docs/screenshots/web_settings.png) |

---

## 🔌 Boot modes

What is pressed while you plug in the USB-C cable selects the mode for that session:

| Held while plugging in | Result |
| --- | --- |
| nothing | Normal: serial port, MIDI and USB Ethernet (as enabled in `config.txt`) |
| **both paddles** | Normal **plus** a USB drive named `SMARTCW` containing `config.txt` |
| **PTT button** | Normal **plus** a USB keyboard: decoded CW is typed as keystrokes, PTT = Enter |

Keyboard mode can be made permanent with `keyboard: true`. The drive can also be exposed at runtime with the `!drive` command.

When the host **ejects** the `SMARTCW` drive, SmartCW reboots and applies the edited `config.txt`. Use the file manager's *Eject* (macOS / Windows / GNOME), not a plain unmount: a bare `umount` does not send the SCSI eject, so send `!reboot` or re-plug the cable instead. Avoid *Safely Remove / power off* on Linux hubs with per-port power switching: it cuts power to the device. While the drive is exposed SmartCW never writes to its own filesystem, so edits from the host cannot collide with its own.

---

## 🔘 PTT button

| Press | Normal mode | Keyboard mode |
| --- | --- | --- |
| short (released before 1.5 s) | MIDI PTT pulse: note `midiPttNote` on, off 150 ms later; PWR + SER2 LEDs flash | types Enter |
| long (held 1.5 s) | toggles straight key and the paddle keyer: 1 beep = keyer, 2 beeps = straight; saved | same |

## 🔑 Key types and output

The paddle jack takes paddles (dit on tip, dah on ring, `paddleSwap` for left-handers) or a straight key (tip, ring or both closed = key down, so a mono plug works).

| `keyer` | Behaviour |
| --- | --- |
| `iambicA` | squeeze keying, no extra element when you release both paddles |
| `iambicB` | squeeze keying with the extra opposite element (the classic Curtis B) |
| `ultimatic` | when both paddles are held, the last one pressed wins |
| `bug` | dit paddle sends automatic dits, dah paddle is a manual key |
| `straight` | everything follows the key; decoding adapts to your speed |

| `output` | The output jack | Line out |
| --- | --- | --- |
| `key` | keys the radio like a straight key | sidetone |
| `audio` | is PTT: on `pttLead` ms before the first tone, off `pttTail` ms after the last | the CW tone that goes on air |
| `off` | idle (practice) | sidetone |

All timing runs in a 1 kHz interrupt, independent of USB and network traffic: at 20 wpm dits are 60 ms, dahs 180 ms, gaps 60 ms, measured over MIDI.

---

## 🛠 Configuration – `config.txt`

Every setting can be changed in the web interface, with `!<setting> <value>` over serial or TCP (`!wpm 22`), or by editing `config.txt` on the `SMARTCW` drive. The file documents itself; delete it and eject to regenerate the defaults. Settings marked *restart* change the USB layout and apply after a restart.

| Setting | Default | Range / options | What it does |
| --- | --- | --- | --- |
| `keyer` | `iambicB` | iambicA, iambicB, ultimatic, bug, straight | key type on the paddle jack; a long press (1.5 s) on PTT toggles straight |
| `wpm` | `18` | 5..60 | keyer and text speed, words per minute |
| `weight` | `50` | 25..75 | mark/space weighting in %, 50 = standard |
| `ratio` | `30` | 25..45 | dah length in tenths of a dit, 30 = 3.0 |
| `paddleSwap` | `off` | on, off | swap the dit and dah paddles |
| `autoSpace` | `off` | on, off | iambic: wait for a full letter space after a pause |
| `tone` | `600` | 300..2000 | sidetone frequency in Hz |
| `buzzer` | `on` | on, off | piezo buzzer on/off |
| `volume` | `10` | 1..10 | buzzer level 1..10 |
| `lineLevel` | `10` | 1..10 | line out level 1..10 |
| `output` | `key` | key, audio, off | key = output keys the radio, audio = output is PTT and the tone goes out on line out, off = practice |
| `pttLead` | `50` | 0..500 | audio mode: ms between PTT and the first tone |
| `pttTail` | `400` | 0..3000 | audio mode: ms PTT stays on after the last tone |
| `farnsworth` | `0` | 0..60 | text sending: effective wpm for the spacing, 0 = off |
| `trainWpm` | `20` | 5..60 | trainer character speed, wpm |
| `trainEff` | `12` | 0..60 | trainer effective (Farnsworth) speed, 0 = off |
| `trainTone` | `0` | 0..2000 | trainer tone in Hz, 0 = same as the sidetone |
| `midi` | `on` | on, off | USB MIDI port (restart) |
| `midiMode` | `keyed` | keyed, paddles | keyed = keyer output as one note, paddles = raw paddle contacts as two notes |
| `midiCwNote` | `65` | 0..127 | note held while the keyer or text keys down |
| `midiStraightNote` | `64` | 0..127 | note held while the straight key is closed |
| `midiPttNote` | `66` | 0..127 | note pulsed by the PTT button, held during audio-mode PTT |
| `midiDitNote` | `67` | 0..127 | paddles mode: dit contact |
| `midiDahNote` | `68` | 0..127 | paddles mode: dah contact |
| `keyboard` | `off` | on, off | type decoded CW as USB keystrokes (holding PTT while plugging in does this once) (restart) |
| `call` | `(empty)` | text, 15 chars | your callsign |
| `name` | `(empty)` | text, 19 chars | your name |
| `mem1` | `CQ CQ CQ DE {call} {call} K` | text, 71 chars | memory 1, {call} and {name} are filled in |
| `mem2` | `{call}` | text, 71 chars | memory 2, {call} and {name} are filled in |
| `mem3` | `TU 5NN` | text, 71 chars | memory 3, {call} and {name} are filled in |
| `mem4` | `R TNX FER QSO 73 <SK>` | text, 71 chars | memory 4, {call} and {name} are filled in |
| `mem5` | `QRL?` | text, 71 chars | memory 5, {call} and {name} are filled in |
| `mem6` | `{call} DE {call}` | text, 71 chars | memory 6, {call} and {name} are filled in |
| `beaconText` | `VVV VVV DE {call} {call}` | text, 71 chars | beacon message |
| `beaconEvery` | `60` | 5..3600 | seconds between beacon transmissions |
| `network` | `on` | on, off | USB Ethernet with the web interface and TCP port (restart) |
| `ip` | `192.168.7.1` | text, 15 chars | SmartCW's address; the computer gets the next free one from its DHCP server (/24) (restart) |
| `tcpPort` | `23` | 1..65535 | TCP port for the text/command protocol (restart) |

Memories, the beacon text and the trainer understand `{call}` and `{name}`, and prosigns written as `<SK>`, `<KN>`, `<AR>`, `<BT>`, `<AS>`, `<BK>`, `<HH>`.

---

## 🗣 Serial and TCP protocol

The USB serial port (any baud rate) and TCP port 23 (`nc 192.168.7.1 23`, `telnet 192.168.7.1`) speak the same protocol:

- Any character you send is keyed as CW and echoed back when it is sent.
- Whatever you key on the paddles comes back as decoded text (`¿` for an unknown pattern).
- A line starting with `!` is a command; every response line starts with `!`.

| Command | Effect |
| --- | --- |
| `!help` | list of commands |
| `!status` | settings, USB and network in words |
| `!state` | everything as one JSON line |
| `!get [setting]`, `!<setting> [value]` | show or change any setting, e.g. `!wpm 22`, `!tone 650`, `!keyer iambicA` |
| `!send <text>`, `!mem <1..6>` | send text or a memory |
| `!beacon on/off`, `!tune [s/off]`, `!practice on/off` | beacon, tune (default 5 s), practice mode |
| `!ptt`, `!stop` | PTT pulse; abort text, tune and beacon |
| `!save`, `!defaults [all]` | write `config.txt` now (changes also save themselves); factory settings, `all` also clears call and memories |
| `!net`, `!drive`, `!reboot`, `!bootloader` | network status, expose the drive, restart, firmware update mode |
| `!perf` | longest main-loop stalls since the last `!perf` (diagnostics) |

---

## 🎹 MIDI

With `midi: true` SmartCW presents a USB MIDI port named `SmartCW`, channel 1:

| Event | MIDI (defaults) |
| --- | --- |
| key down / up, paddles or text, `midiMode: keyed` | note on / off `midiCwNote` (65), velocity 127 |
| straight key closed / open | note on / off `midiStraightNote` (64) |
| dit / dah paddle contact, `midiMode: paddles` | note on / off `midiDitNote` (67) / `midiDahNote` (68) |
| PTT button | note `midiPttNote` (66) on, off 150 ms later |
| audio-mode PTT | note `midiPttNote` held while PTT is on |

Nothing is sent over MIDI in practice mode or while a trainer page is open.

---

## 💡 LEDs

| LED | Meaning |
| --- | --- |
| PWR | on when running; off while text is being sent |
| DIT | on while a dit is keyed (or the straight key is closed) |
| DAH | on while a dah is keyed |
| SER | on while text, a memory or the beacon is being sent |
| SER2 | audio-mode PTT; flashes with PWR when PTT is pressed |

---

## 🔁 Firmware update

Pre-built UF2 files are attached to every [GitHub release](https://github.com/Guru-RF/SmartCW/releases). Settings are kept.

- **Web**: System → *Restart into update mode*, then drag `smartcw-*.uf2` onto the `RPI-RP2` drive.
- **Command**: send `!bootloader` over serial or TCP, then drag the UF2 onto `RPI-RP2`.
- **Config file**: boot with both paddles held, replace the contents of `config.txt` with the single word `firmwareupdate`, eject.
- **BOOTSEL (recovery)**: unplug, hold BOOTSEL, plug in, release; drag the UF2 onto `RPI-RP2`.

`flash_nuke.uf2` (shipped with every release) erases the whole flash, including `config.txt`; flash the regular firmware afterwards.

---

## 🧑‍💻 Building from source

Requires [PlatformIO](https://platformio.org/install) (`pip install platformio`).

```bash
pio run -e pico                 # build -> .pio/build/pico/firmware.uf2
pio run -e pico -t upload       # build + flash (device in RPI-RP2 mode)
pio device monitor              # serial console
```

The web interface lives in `web/` (plain HTML, CSS and JavaScript, no build tools). `tools/build_web.py` runs before every build: it inlines the scripts and styles into one page, gzips it and generates `src/web_assets.h`.

Every push to `main` and every tag is built by GitHub Actions (`.github/workflows/build.yml`); a tag like `v2.1.0` attaches the UF2 to the matching release.

### Source layout

| File | Role |
| --- | --- |
| `src/main.cpp` | boot modes, start-up, main loop |
| `src/engine.*` | real-time engine in a 1 kHz interrupt: keyer modes, decoder, text player, tune, beeps |
| `src/output.*` | sidetone, key line, audio-mode PTT with lead and tail |
| `src/hub.*` | engine events to MIDI, serial, TCP, web and keyboard; beacon, memories |
| `src/config.*` | settings table, `config.txt` reader/writer, JSON, auto-save |
| `src/cli.*` | the serial/TCP protocol and `!` commands |
| `src/net.*` | USB Ethernet (CDC-NCM, IPv4 + IPv6 link-local), DHCP server, mDNS / DNS-SD, TCP port |
| `src/crash.*` | remembers a panic or hard fault across the restart, reported by `!status` |
| `src/web.*` | web server, JSON API, live event stream |
| `src/dhcps.*` | DHCP server (from MicroPython, without gateway and DNS) |
| `src/morse.*` | Morse table with prosigns |
| `src/ptt.*`, `src/leds.*`, `src/usbio.*` | PTT button, LEDs, USB MIDI / keyboard / drive |
| `web/` | the web interface; `web/js/data.js` holds the trainer content |
| `tools/build_web.py` | bundles `web/` into the firmware |

### Hardware map

| Function | GPIO |
| --- | --- |
| Dit paddle | 13 |
| Dah paddle | 12 |
| PTT button | 16 |
| CW key / PTT output | 14 |
| Sidetone piezo | 24 |
| Sidetone / TX audio line out | 15 |
| LED PWR / DAH / DIT / SER / SER2 | 3 / 8 / 11 / 10 / 7 |

Inputs are active-low with internal pull-ups; LEDs are active-low.

---

## Credits

- The iambic keyer logic started from [xiaoKey](https://github.com/MarkWoodworth/xiaokey) by Mark Woodworth (AC9YW), MIT licensed.
- The DHCP server is MicroPython's (Damien P. George), MIT licensed, as shipped in arduino-pico.
- The visual Morse tree layout follows the classic dichotomic Morse tree.
