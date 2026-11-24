# 2. Keying

## Key types

Set the key type in Settings → Keyer, or with `!keyer <type>`.

| Type | Behaviour |
| --- | --- |
| **Iambic B** (default) | Squeeze keying; releasing both paddles adds one opposite element, the classic Curtis B |
| **Iambic A** | Squeeze keying without the extra element |
| **Ultimatic** | With both paddles held, the one pressed last repeats |
| **Bug** | The dit paddle makes automatic dits at the set speed; the dah paddle is a hand key |
| **Straight** | Everything follows the key, as a straight key, cootie or a bug's output |

A long press on the PTT button (1.5 s) switches between the straight key and the paddle type you had: one beep means keyer, two beeps mean straight key. The change is saved.

## Speed and feel

- **Speed** (`wpm`, 5 to 99): on the Live page, in Settings, with PTT + paddle on the box, or `!wpm 22`. One speed serves the keyer, text sending and the trainer.
- **Weight** (`weight`, 25 to 75 %, 50 = standard): makes dits and dahs longer or shorter against the spaces.
- **Dah ratio** (`ratio`, 2.5 to 4.5, written as tenths: 30 = 3.0).
- **Paddle swap** (`paddleSwap`) for left-handers.
- **Autospace** (`autoSpace`): iambic modes wait for a full letter space after a pause, so letters never run together.

All timing runs in a 1 kHz interrupt, independent of USB and network traffic.

## Your fist

A keyer makes perfect 1:3 marks, a hand does not. When you key by hand, a straight key or a bug's dahs, SmartCW decodes you against your own timing. It learns four lengths while you key: your dit, your dah, the gap you leave inside a letter and the gap between letters. Dahs of 2.2 dits or of 4, letters run together at two dits or spread out at four: SmartCW follows within a few letters, and follows a change of operator too. On paddles the marks are the keyer's, but the letter and word spacing is still yours and is learned the same way.

The trainer's **Fist** page shows what SmartCW has learned, and colours every letter it reads by how sure it was: green when the marks and gaps sat squarely in your pattern, red when one sat on the line between a dit and a dah or between two letters. **Save as my fist** keeps the four lengths in `config.txt`, so the decoder starts from your fist at every power-up instead of from the keyer speed; **Forget** clears them. On the serial port, `!fist` shows the live values, `!fist save` and `!fist forget` do the same as the buttons.

## Sending text

Anything typed on the Live page, on the serial port or on TCP is sent as CW at the keyer speed, and echoed as it goes out. Prosigns are written in angle brackets: `<SK>`, `<KN>`, `<AR>`, `<BT>`, `<AS>`, `<BK>`, `<HH>`. **Farnsworth** (`farnsworth`) sends the characters at the keyer speed and stretches the spaces to a lower effective speed.

The paddles always win: pressing a paddle stops any text, memory, beacon or tune in progress.

## Memories

Ten memories, sent from the Live page, with `!mem 1` to `!mem 10` on the serial port or TCP, or from a logger. They understand:

| Written | Sent as |
| --- | --- |
| `{call}`, `{name}` | your callsign and name from Settings |
| `{nr}` | the contest serial number, three digits (`001`), and moves on to the next number once the memory is sent |
| `{lastnr}` | the previous serial number again, when someone asks for a repeat |
| `<SK>` and the other prosigns | the prosign |

Set the serial number for a new contest on the Live page, in Settings or with `!nr 1`. **Cut numbers** (`cutNumbers`) send 0 as T, and with `tn` also 9 as N.

The defaults are a CQ, your call, `TU 5NN`, a sign-off, `QRL?`, `{call} DE {call}`, `5NN {nr}`, `{lastnr} {lastnr}`, `AGN?` and `QRZ?`.

## Beacon and tune

- **Beacon** repeats `beaconText` every `beaconEvery` seconds (5 to 3600), for antenna tests or fox hunts. Start it on the Live page or with `!beacon on`; a paddle press or `!beacon off` stops it.
- **Tune** keys down for 5 seconds from the Live page, or for up to 20 seconds with `!tune 10`. `!tune off`, `!stop` or a paddle ends it early.

## The PTT button

| Press | Normal mode | Keyboard mode |
| --- | --- | --- |
| short, under 1.5 s | a PTT pulse over MIDI (note `midiPttNote`), PWR blinks | types Enter |
| long, 1.5 s | toggles straight key and paddle keyer; 1 beep keyer, 2 beeps straight | same |
| held, tap a paddle | dit = 1 wpm slower, dah = 1 wpm faster; the new speed is played, never transmitted | same |
| held, squeeze both paddles | the menu, see below | same |

While a contest logger has SmartCW open as a WinKeyer, the speed change is reported as the WinKeyer speed pot and the button as pushbutton 1.

## The menu on the box

The settings you reach for at the key are on the box itself, without a computer. Hold the PTT button and squeeze both paddles: SmartCW answers **VOL** in CW on the sidetone. While the menu is open nothing is transmitted and the paddles key nothing.

- **Dah** goes to the next item, **dit** to the previous one; each is said by name: VOL (buzzer level), LINE (line out level, the headphones), TONE (pitch, 50 Hz a step), KEY (keyer: A, B, U for Ultimatic, BUG, STR for straight), SWAP (the paddles swapped), OUT (KEY, AUD for audio + PTT, OFF for practice), BUZ (buzzer on or off), KBD (USB keyboard on or off).
- A **short press** opens the item and says its value. Dah steps it up, dit down; a switch flips on either paddle. Every step is said, and you hear it at the new setting: the volume at the new volume, the pitch at the new pitch.
- A **short press** again saves it. SmartCW says **R** and the menu closes. KBD restarts SmartCW after the R, as the USB layout changes.
- A **long press**, or 30 seconds of silence, puts the value back and closes the menu with **SK**.

The web page and the serial console follow along: a saved item appears there as `!menu volume 5`.
