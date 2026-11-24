# 7. Reference

## Every setting: `config.txt`

Every setting can be changed on the Settings page, with `!<setting> <value>` on the serial port or TCP (`!wpm 22`), or by editing `config.txt` on the `SMARTCW` drive (hold both paddles while plugging in, edit, eject). The file documents itself; delete it and eject to get the defaults back. Settings marked *restart* change the USB layout and apply after a restart.

| Setting | Default | Range / options | What it does |
| --- | --- | --- | --- |
| `keyer` | `iambicB` | iambicA, iambicB, ultimatic, bug, straight | key type on the paddle jack; a long press (1.5 s) on PTT toggles straight |
| `wpm` | `18` | 5..99 | keyer, text and trainer speed, words per minute |
| `weight` | `50` | 25..75 | mark/space weighting in %, 50 = standard |
| `ratio` | `30` | 25..45 | dah length in tenths of a dit, 30 = 3.0 |
| `paddleSwap` | `off` | on, off | swap the dit and dah paddles |
| `autoSpace` | `off` | on, off | iambic: wait for a full letter space after a pause |
| `fistDit`, `fistDah`, `fistGap`, `fistCharGap` | `0` | ms | your fist as saved by `!fist save` or the trainer's Fist page: dit, dah, gap inside a letter, gap between letters; 0 = none, the decoder starts from `wpm`; kept by `!defaults` |
| `tone` | `600` | 300..2000 | sidetone frequency in Hz |
| `buzzer` | `on` | on, off | piezo buzzer on/off |
| `volume` | `10` | 1..10 | buzzer level 1..10 |
| `lineLevel` | `10` | 1..10 | line out level, 4 dB a step: 10 drives a radio, headphones want 3 to 6 |
| `lineSidetone` | `off` | on, off | audio mode: the sidetone on line out as well as the TX tone (key and off modes always have it there) |
| `output` | `key` | key, audio, off | key = output keys the radio, audio = output is PTT and the tone goes out on line out, off = practice |
| `pttLead` | `50` | 0..500 | audio mode: ms between PTT and the first tone |
| `pttTail` | `400` | 0..3000 | audio mode: ms PTT stays on after the last tone |
| `keyComp` | `0` | 0..50 | ms added to every transmitted dit and dah (QSK), taken from the space after it |
| `firstExt` | `0` | 0..150 | ms added to the first dit or dah after a pause, for radios that clip it |
| `farnsworth` | `0` | 0..60 | text sending: effective wpm for the spacing, 0 = off |
| `trainEff` | `12` | 0..60 | trainer effective (Farnsworth) speed, 0 = off |
| `trainTone` | `0` | 0..2000 | trainer tone in Hz, 0 = same as the sidetone |
| `midi` | `on` | on, off | USB MIDI port (restart) |
| `midiMode` | `keyed` | keyed, paddles | keyed = keyer output as one note, paddles = raw paddle contacts as two notes |
| `midiCwNote` | `65` | 0..127 | note held while the keyer or text keys down |
| `midiStraightNote` | `64` | 0..127 | note held while the straight key is closed |
| `midiPttNote` | `66` | 0..127 | note pulsed by the PTT button, held during audio-mode PTT |
| `midiDitNote` | `67` | 0..127 | paddles mode: dit contact |
| `midiDahNote` | `68` | 0..127 | paddles mode: dah contact |
| `keyboard` | `off` | on, off | USB keyboard (holding PTT while plugging in turns it on once) (restart) |
| `keyboardMode` | `text` | text, ctrl | text = decoded CW is typed, PTT = Enter; ctrl = paddles as Left/Right Ctrl keys for vBand and Vail |
| `winkeyer` | `on` | on, off | answer WinKeyer commands on the USB serial port, for contest loggers |
| `call` | `(empty)` | text, 15 chars | your callsign |
| `name` | `(empty)` | text, 19 chars | your name |
| `mem1` | `CQ CQ CQ DE {call} {call} K` | text, 71 chars | memory 1, {call}, {name} and {nr} are filled in |
| `mem2` | `{call}` | text, 71 chars | memory 2, {call}, {name} and {nr} are filled in |
| `mem3` | `TU 5NN` | text, 71 chars | memory 3, {call}, {name} and {nr} are filled in |
| `mem4` | `R TNX FER QSO 73 <SK>` | text, 71 chars | memory 4, {call}, {name} and {nr} are filled in |
| `mem5` | `QRL?` | text, 71 chars | memory 5, {call}, {name} and {nr} are filled in |
| `mem6` | `{call} DE {call}` | text, 71 chars | memory 6, {call}, {name} and {nr} are filled in |
| `mem7` | `5NN {nr}` | text, 71 chars | memory 7, {call}, {name} and {nr} are filled in |
| `mem8` | `{lastnr} {lastnr}` | text, 71 chars | memory 8, {call}, {name} and {nr} are filled in |
| `mem9` | `AGN?` | text, 71 chars | memory 9, {call}, {name} and {nr} are filled in |
| `mem10` | `QRZ?` | text, 71 chars | memory 10, {call}, {name} and {nr} are filled in |
| `nr` | `1` | 1..9999 | contest serial number, {nr} sends it and moves on, {lastnr} repeats the last one |
| `cutNumbers` | `off` | off, t, tn | serial numbers: t = send 0 as T, tn = also 9 as N |
| `beaconText` | `VVV VVV DE {call} {call}` | text, 71 chars | beacon message |
| `beaconEvery` | `60` | 5..3600 | seconds between beacon transmissions |
| `network` | `on` | on, off | USB Ethernet with the web interface and TCP port (restart) |
| `ip` | `192.168.7.1` | text, 15 chars | SmartCW's address; the computer gets the next free one from its DHCP server (/24) (restart) |
| `tcpPort` | `23` | 1..65535 | TCP port for the text/command protocol (restart) |
| `cwdaemonPort` | `6789` | 0..65535 | cwdaemon UDP port for Linux loggers, 0 = off (restart) |

Memories, the beacon text and the trainer understand `{call}` and `{name}`, and prosigns written as `<SK>`, `<KN>`, `<AR>`, `<BT>`, `<AS>`, `<BK>`, `<HH>`. Memories also understand `{nr}`, the contest serial number, sent as three digits (`001`, or `TT1` with `cutNumbers: t`), which moves on once the memory is sent, and `{lastnr}`, the previous one.

## Every command

On the serial port (any baud rate), on TCP port 23 and in the System page's console. Any other text is sent as CW; what you key comes back as text, `¿` for an unknown pattern. Every answer starts with `!`.

| Command | Effect |
| --- | --- |
| `!help` | list of commands |
| `!status` | settings, USB and network in words |
| `!state` | everything as one JSON line |
| `!get [setting]`, `!<setting> [value]` | show or change any setting, e.g. `!wpm 22`, `!tone 650`, `!keyer iambicA` |
| `!send <text>`, `!mem <1..10>` | send text or a memory |
| `!nr [n]` | show or set the contest serial number |
| `!beacon on/off`, `!tune [s/off]`, `!practice on/off` | beacon, tune (default 5 s), practice mode |
| `!ptt`, `!stop` | PTT pulse; abort text, tune and beacon |
| `!save`, `!defaults [all]` | write `config.txt` now (changes also save themselves); factory settings, `all` also clears call and memories |
| `!net`, `!drive`, `!reboot`, `!bootloader` | network status, expose the drive, restart, firmware update mode (`!reboot` and `!bootloader` save a pending change first) |
| `!version`, `!id` | firmware version; the RF.Guru serial number (`!id set <serial>` on the bench, also the USB serial number; firmware updates and factory settings keep it) |
| `!inputs`, `!selftest` | hardware check: raw paddle and PTT contacts (`!inputs dit 0 dah 0 ptt 0`); each LED in turn and a beep, then `!selftest done` |
| `!perf` | longest main-loop stalls since the last `!perf` (diagnostics) |
| `!fist`, `!fist save`, `!fist forget` | the fist the decoder has learned (dit, dah, gap, letter gap in ms); save it to `config.txt`; clear it |
