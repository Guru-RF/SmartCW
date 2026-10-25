# SmartCW – Interoperability

How SmartCW talks to the computer: USB MIDI, USB keyboard, the serial and TCP text protocol, WinKeyer, cwdaemon and the web API. For the hardware, the settings and building see [README.md](README.md).

SmartCW is a composite USB device (VID `0x1209`, PID `0xFF42`, product string `SmartCW`, manufacturer `RF.Guru`). The USB serial number is the RF.Guru serial number (`!id`), or the RP2040's unique ID on a board that has none. Which interfaces it presents depends on `config.txt` and on what is held while plugging in:

| Interface | When present |
| --- | --- |
| USB serial (CDC-ACM) | always |
| USB MIDI | `midi: true` (default) |
| USB Ethernet (CDC-NCM) | `network: true` (default) |
| USB HID keyboard | `keyboard: true`, or PTT held while plugging in; the PID becomes `0xFF43` |
| USB mass storage | both paddles held while plugging in, or the `!drive` command |

All interfaces are registered right after power-up, before the host has finished enumerating, so the computer normally sees the complete device once. Changing `midi`, `keyboard`, `network`, `ip`, `tcpPort` or `cwdaemonPort` needs a restart.

---

## 🌐 USB network

SmartCW is a CDC-NCM network adapter, supported natively by Linux, macOS and Windows 10/11 (no driver, no Internet Connection Sharing).

| | |
| --- | --- |
| SmartCW's address | `192.168.7.1` (setting `ip`), netmask `255.255.255.0` |
| Your computer | gets `192.168.7.16`–`.23` from SmartCW's DHCP server; or set a static address in the same /24 |
| Gateway, DNS | none are handed out, so the computer keeps its own default route and name servers |
| Name | `smartcw.local` over mDNS: A (192.168.7.1) and AAAA (SmartCW's IPv6 link-local address), so lookups never wait for a missing IPv6 answer |
| DNS-SD | `SmartCW._http._tcp.local` (port 80, TXT `path=/`, `version=`) and `SmartCW._telnet._tcp.local` (the TCP port) |
| IPv6 | link-local only (`fe80::…`, from the MAC address); the web server and the TCP port answer on it too |
| Web interface and API | `http://192.168.7.1/`, port 80 |
| Text protocol | TCP port 23 (setting `tcpPort`) |

lwIP is built with ten TCP connections. The web server handles six at once, up to three of them live event streams (one per open browser tab); the TCP port accepts two clients.

---

## 🎹 MIDI

SmartCW presents one USB MIDI port named **SmartCW** (on Linux ALSA lists it as `SmartCW virtual-cable`). Everything is sent on **MIDI channel 1** with velocity 127 (status `0x90` / `0x80`). Incoming MIDI is ignored.

### Events

| Source | Note on | Note off |
| --- | --- | --- |
| Keyer element or text, `midiMode: keyed` | `midiCwNote` (default **65**) | when the element ends |
| Straight key closed, `midiMode: keyed` | `midiStraightNote` (default **64**) | when the key opens |
| Dit paddle contact, `midiMode: paddles` | `midiDitNote` (default **67**) | when the paddle is released |
| Dah paddle contact, `midiMode: paddles` | `midiDahNote` (default **68**) | when the paddle is released |
| PTT button or `!ptt` | `midiPttNote` (default **66**) | 150 ms later |
| Audio-mode PTT (`output: audio`) | `midiPttNote` (66) | after `pttTail` |

**Keyed mode**: SmartCW is the keyer. The SDR receives fully timed CW, one note per dit or dah, from the paddles, the straight key, text, memories and the beacon. **Paddles mode**: the SDR is the keyer. It receives the raw paddle contacts and does the timing itself; SmartCW's own sidetone and key output keep working.

Nothing is sent over MIDI while practice mode is active (`output: off`, `!practice on`, or a trainer page open in the browser).

The PTT button sends a **pulse**: map it to a toggle in your software. Holding it for 1.5 s switches key type instead.

### Mapping in SDR software

1. Open the program's MIDI or controller settings and select the `SmartCW` input.
2. Keyed mode: map note 65 (and 64 for a straight key) to "CW key", note 66 to "PTT toggle". If the program has one CW input only, set `midiStraightNote` equal to `midiCwNote`.
3. Paddles mode: map note 67 to "dit paddle" and 68 to "dah paddle".

### Vail

The Vail adapter sends note 0 for keyed output and the straight key, and notes 1 and 2 for the raw dit and dah paddles. *Vail notes* in Settings → USB MIDI sets `midiCwNote` and `midiStraightNote` to 0, `midiDitNote` to 1 and `midiDahNote` to 2 (*SmartCW notes* puts the defaults back). With `midiMode: keyed` choose *Straight key* as Vail's keyer; with `midiMode: paddles` Vail keys. Vail's MIDI control messages (speed, tone, keyer type) are ignored: set those on SmartCW.

### Testing on a computer

```bash
amidi -l                       # find the port, e.g. hw:3,0,0
amidi -p hw:3,0,0 -d           # dump events, then key something
```

```text
90 41 7F    note on 65     (a dit)
80 41 00    note off 65
90 42 7F    note on 66     (PTT pulse)
80 42 00    note off 66
```

At 20 wpm the note-on time of a dit measures 60 ms, a dah 180 ms, the gap between elements 60 ms and between letters 180 ms.

---

## ⌨️ USB keyboard mode

SmartCW also enumerates as a standard HID keyboard (US layout) and types what it **decodes from the paddles or straight key** into the window with keyboard focus:

- Every decoded character is typed: lowercase letters, digits and the punctuation of the Morse table. A word gap types a space.
- Undecodable patterns and the error signal (`........`) are not typed.
- Text sent from the computer is keyed, but not typed back.
- A short press on PTT types **Enter**.
- Nothing is typed while practice mode is active.

Turn it on for one session by holding PTT while plugging in, or permanently with `keyboard: true` (web: Settings → Computer, then restart). On a host with a non-US layout some punctuation may come out as a different symbol; letters and digits are unaffected.

### Paddle keys for vBand and Vail (`keyboardMode: ctrl`)

With `keyboardMode: ctrl` the keyboard types nothing; instead the paddle contacts hold keys, the convention of the vBand and Vail USB adapters:

| Contact | Key |
| --- | --- |
| dit paddle | Left Ctrl, held while the paddle is closed |
| dah paddle | Right Ctrl, held while the paddle is closed |
| straight key (`keyer: straight`) | Left Ctrl |

These are the raw contacts (after `paddleSwap`): the website does the keying. SmartCW's own sidetone and keyer keep running, so turn `buzzer` off or set `output: off` if you only want the website. No keys are held while practice mode is active (a trainer page is open). On Linux, keyd's default config maps Right Ctrl to Left Ctrl; keep Right Ctrl as itself for the dah paddle.

---

## 🔑 Key types

| `keyer` | Behaviour |
| --- | --- |
| `iambicA` | iambic squeeze keying, mode A: no extra element after releasing both paddles |
| `iambicB` | iambic mode B (default): the opposite element is added after a squeeze release |
| `ultimatic` | both paddles held: the paddle pressed last repeats |
| `bug` | dit paddle = automatic dits, dah paddle = manual key |
| `straight` | tip or ring closed = key down; marks follow the key exactly |

The keyer decodes exactly from its own elements. The straight key and the bug's manual dahs are decoded adaptively: SmartCW tracks your dit length, so `wpm` is only the starting point. A long press (1.5 s) on PTT toggles between `straight` and the last paddle mode (one beep = keyer, two = straight) and is reported on the serial port and TCP as `!keyer ...`.

---

## 🔌 Serial and TCP protocol

The USB serial port (`/dev/ttyACM*`, `/dev/tty.usbmodem*`, `COMx`; any baud rate, DTR asserted) and the TCP port (`nc 192.168.7.1 23`, `telnet 192.168.7.1`) speak the same protocol. Telnet option negotiation is ignored. On the serial port a `0x00` or `0x13` byte switches to the WinKeyer protocol (see below) until the logger closes it.

### Data stream

- **To SmartCW**: every printable character is keyed as CW. Line breaks and tabs become word spaces. `<SK>`, `<KN>`, `<AR>`, `<BT>`, `<AS>`, `<BK>`, `<HH>` send prosigns; any other `<XYZ>` sends the letters run together.
- **From SmartCW**: each character is echoed when it is actually keyed (lowercase); whatever is keyed on the paddles comes back decoded, `¿` for an unknown pattern, a space after a word gap.

Prosigns travel as single stand-in characters in both directions:

| Prosign | `<AR>` | `<KN>` | `<SK>` | `<BT>` | `<AS>` | `<BK>` | `<SN>` | `<HH>` (error) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Character | `+` | `(` | `\|` | `=` | `&` | `^` | `*` | `#` |

### Commands

A line starting with `!` is a command, ended by CR or LF. Every response line starts with `!`; errors are `!err <message>`.

| Command | Response / effect |
| --- | --- |
| `!help` | the command list |
| `!status` | settings, USB and network in words (second line: `!id <serial or none>, chip <chip ID>`); after an unexpected restart also `!last crash: ...` |
| `!state` | `!state {...}`: the same JSON as `GET /api/state` |
| `!version` | `!version 2.3.1` |
| `!id` | `!id SCW-00042`: the RF.Guru serial number, or `!id none` when the board has none yet |
| `!id set <serial>` | store the serial number (1–24 characters, A–Z, 0–9 and `-`, lowercase is raised); replies `!id <serial>`. Refused with `!err id already set: !id clear first` while another one is stored. It is kept in its own flash sector: firmware updates, `config.txt` and factory settings never touch it, only `flash_nuke.uf2` erases it. From the next restart it is also the USB serial number |
| `!id clear` | remove it; replies `!id none`. The USB serial number goes back to the chip ID after a restart |
| `!get` | `!<setting> <value>` for every setting |
| `!get <setting>`, `!<setting>` | one setting |
| `!set <setting> <value>`, `!<setting> <value>` | change a setting, e.g. `!wpm 22`, `!keyer iambicA`, `!output audio`, `!mem1 CQ TEST {call}` |
| `!send <text>` | send text (same as typing it) |
| `!mem <1..10>` | send a memory; `{nr}` in it sends the serial number, which then moves on |
| `!nr [n]` | show or set the contest serial number (a setting like any other) |
| `!beacon on/off` | repeat `beaconText` every `beaconEvery` seconds |
| `!tune [1..20 / off]` | key down to tune, default 5 s |
| `!practice on/off` | no TX for 10 minutes, or until off |
| `!ptt` | MIDI PTT pulse (Enter in keyboard mode) |
| `!stop` | abort text, tune and beacon |
| `!save` | write `config.txt` now; changes are also saved by themselves 2 s after the last change |
| `!defaults` / `!defaults all` | factory settings; without `all`, call, name, memories and beacon text are kept |
| `!net` / `!net tcp` | web address, TCP port, clients; `tcp` lists every TCP connection |
| `!perf` | the longest main-loop stalls (µs) since the last `!perf` |
| `!drive` | expose `config.txt` as a USB drive now (USB re-enumerates) |
| `!inputs` | `!inputs dit 0 dah 0 ptt 0`: the raw contacts as wired (before `paddleSwap`), 1 = closed |
| `!selftest` | each LED in turn (PWR, DIT, DAH, SER, SER2, 250 ms each), then all LEDs and a beep on the buzzer (even with `buzzer: false`); about 2 s later `!selftest done` goes to the serial port and every TCP client |
| `!reboot` | restart; writes `config.txt` first when a change is pending |
| `!bootloader` | `!rebooting into UF2 bootloader`, then the UF2 bootloader (`RPI-RP2`); writes `config.txt` first when a change is pending, so a setting changed just before an update is kept |

The boot banner, sent about 1.5 s after power-up, starts with `!SmartCW 2.3.1 ready` and ends with `!type text to send CW, !help for commands`. A TCP client gets a one-line greeting.

### Minimal client (Python)

```python
import socket
s = socket.create_connection(("192.168.7.1", 23))
s.sendall(b"!wpm 22\ncq cq de on6ure k\n")
while True:
    print(s.recv(256).decode("utf-8", "replace"), end="", flush=True)
```

For the serial port use `serial.Serial("/dev/ttyACM0", 115200)` from pyserial the same way.

---

## 🏆 WinKeyer

The USB serial port answers the K1EL WinKeyer host protocol, so contest loggers can use SmartCW wherever they support a WinKeyer. SmartCW behaves as a **WinKeyer 2, version 23** (`Host Open` returns 23) and also accepts the WK3 admin commands.

- **Switching over.** The first `0x00` (admin) or `0x13` (null) byte switches the serial port to WinKeyer; from then on SmartCW's own text and `!` lines no longer appear on the serial port (they still go to TCP and the web page). `Host Close`, `Reset`, the logger dropping DTR or, before `Host Open`, printable text after 2 s of silence switch back to the text protocol. `winkeyer: false` turns all of this off.
- **1200 baud.** Loggers open the port at 1200 baud and drop DTR when they close it, which on an RP2040 normally means "reboot into the bootloader". With `winkeyer: true` that trigger is disabled; use `!bootloader`, the web page or BOOTSEL for firmware updates.
- **Settings.** While a logger has the interface open, its `Load Defaults` and individual commands set speed (5–99 wpm, `0` = SmartCW's own speed), weight, dah ratio, sidetone frequency (WK2 table, or 62500/n in WK3 mode), sidetone on/off (PINCFG bit 1 = `buzzer`), key mode and paddle swap, autospace, contest spacing, PTT lead and tail (`pttLead`, `pttTail`), keying compensation (`keyComp`) and first extension (`firstExt`). They are not written to `config.txt`; SmartCW's own values come back at `Host Close`. A straight key stays straight whatever the key mode says. A setting changed on the web page or by command during the session is yours and stays.
- **Text.** Letters, digits and the WinKeyer punctuation and prosign mappings (`<` AR, `>` SK, `:` and `]` KN, `[` AS, `;` AA, `\` DN, ...) are queued in a 192-entry buffer and keyed one character at a time. Buffered commands act at their place in the text: speed change and cancel, wait, key down, merge letters. Backspace, Clear Buffer and Pause work.
- **Back to the logger.** Status bytes (busy, break-in, wait, XOFF above 2/3 full, key down in WK1 mode) whenever they change and on request; serial echo after each character is sent and paddle echo of decoded characters when enabled in the mode register; speed pot bytes (`Get Speed Pot`, and whenever the speed is changed with PTT + paddle); `Get Values`; pushbutton status (PTT = pushbutton 1) in WK2 mode.
- **Paddles.** Break-in clears the buffer and drops text from the logger until one word space after the last element, as on a WinKeyer. `Software Paddle` and `Key Immediate` (tune, up to 20 s) work.
- **Not supported.** HSCW, Farnsworth from the logger, paddle-only sidetone, buffered PTT, pointer commands (their bytes are consumed), standalone messages from EEPROM (`Send Message n` sends SmartCW's memory n), RTTY.

## 📡 cwdaemon

With the USB network on, SmartCW listens for [cwdaemon](https://github.com/acerion/cwdaemon) requests on UDP port `cwdaemonPort` (6789, `0` = off).

| Request | Effect |
| --- | --- |
| text | sent as CW; `+` / `-` raise / lower the speed 2 wpm for the rest of that text; `*` AR, `=` BT, `<` SK, `(` KN, `!` SN, `&` AS, `>` BK; `~` is ignored |
| `text^` | as text, and `text` comes back once it has been sent |
| `<ESC>h<reply>` | the next text request replies `h<reply>` once it has been sent |
| `<ESC>0` | stop sending, forget a pending reply |
| `<ESC>2<wpm>` | speed, saved like any setting |
| `<ESC>3<Hz>` | tone; `0` turns the buzzer off, the next tone turns it back on |
| `<ESC>4` | abort; a pending reply is answered with `break` |
| `<ESC>7<-50..50>` | weight (0 = standard) |
| `<ESC>c<s>` | tune, up to 10 s |
| `<ESC>d<ms>` | PTT lead (`pttLead`), 0–50 |
| `<ESC>5 6 8 9 a b e f g` | accepted and ignored |

Replies go to the address and port of the last request, ending in CR LF. A paddle press aborts cwdaemon text like any other text (and answers a pending reply with `break`).

---

## 🧩 Web API

Plain HTTP on port 80. Requests are `GET` or `POST` with url-encoded form fields; responses are JSON unless noted. CORS is open (`Access-Control-Allow-Origin: *`), so your own pages can use the API.

| Request | Fields | Effect / response |
| --- | --- | --- |
| `GET /api/state` | | `{"v", "id", "chip", "up", "cfg": {every setting}, "rt": {"midi", "kbd", "drive", "ip", "beacon", "practice", "busy", "tune", "wk", "restart", "dirty", "web", "tcp"}}`; `v` is the firmware version, `id` the RF.Guru serial number (`""` when not set), `chip` the RP2040's unique ID (16 uppercase hex digits), `wk` is true while a logger has the WinKeyer open |
| `GET /api/meta` | | every setting: key, type, section, help, min/max, default, options, restart flag |
| `POST /api/set` | `setting=value&...` | changes settings; `{"ok", "err": {setting: message}, "restart"}`; valid fields apply even when others fail |
| `POST /api/defaults` | `scope=all` (optional) | factory settings; returns the new state |
| `POST /api/send` | `text`, `p` = `tx` (default), `practice` or `train`, `wpm`, `eff` | queue text; `practice` and `train` are never transmitted, `train` uses the trainer speed and tone, or `wpm`/`eff` when given |
| `POST /api/stop` | | abort text, tune and beacon |
| `POST /api/mem` | `n` = 1..10, `p` | send a memory |
| `POST /api/beacon` | `on` = 1/0 | beacon on or off |
| `POST /api/tune` | `ms` (0 stops, max 20000) | key down to tune |
| `POST /api/practice` | `ms` (default 8000, max 600000) or `on=0` | hold practice mode; the trainer pages refresh this every 4 s |
| `POST /api/ptt`, `POST /api/beep` | `n` for beeps | PTT pulse; sidetone beeps |
| `POST /api/cmd` | `line` = a `!` command | the command's response as text |
| `GET /config.txt` | | the configuration file (download) |
| `POST /config.txt` | the file as the body | restore all settings from it; returns the new state |
| `POST /api/restart`, `POST /api/bootloader` | | restart, or restart into the UF2 bootloader |

### Live events

`GET /api/events` is a Server-Sent Events stream (`EventSource` in a browser). Every message is one JSON object, `e` is its type:

| `e` | Fields | Meaning |
| --- | --- | --- |
| `state` | `s` = same as `/api/state` | sent on connect and after every change |
| `k` | `d` = 1/0, `f` = flags (1 TX, 2 straight key, 4 text player, 8 tune) | key down / up |
| `el` | `v` = `.` or `-`, `m` = mark ms, `g` = gap before it in ms | an element keyed on the paddles or key |
| `rx` | `c` = character, `" "` word gap, `""` unknown | a character decoded from the paddles or key |
| `tx` | `c` = character, `p` = profile (0 tx, 1 train, 2 practice) | the text player starts a character |
| `done` | | the text player ran out of text |
| `abort` | | a paddle interrupted text or the beacon |
| `ptt` | `d` = 1/0 | audio-mode PTT |
| `pulse` | | PTT button pulse |

```js
const es = new EventSource("http://192.168.7.1/api/events");
es.onmessage = (m) => {
  const ev = JSON.parse(m.data);
  if (ev.e === "rx") console.log("decoded", ev.c);
};
fetch("http://192.168.7.1/api/send", { method: "POST", body: new URLSearchParams({ text: "cq test" }) });
```

---

## 💾 Mass storage

Hold both paddles while plugging in (or send `!drive`) and SmartCW also appears as a USB drive labelled `SMARTCW` with `config.txt`. Edit, save and **eject**: the eject (SCSI stop/eject) makes SmartCW reboot and load the file. A plain `umount` on Linux does not send it; re-plug or send `!reboot` instead. While the drive is exposed SmartCW does not write `config.txt` itself: settings changed from the web page or with commands in the meantime are not saved, and the file on the drive wins at the next start.

Replacing the whole file with the single word `firmwareupdate` restarts SmartCW into the UF2 bootloader on the next boot.
