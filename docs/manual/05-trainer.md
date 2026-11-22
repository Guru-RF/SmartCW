# 5. The trainer

The Trainer page holds a complete CW course, played on SmartCW's own sidetone and keyed on your own paddles. Open it at http://smartcw.local/ → Trainer.

**Nothing is transmitted while a trainer page is open.** The pages hold practice mode: the CW jack stays idle and nothing goes over MIDI or the keyboard; SmartCW transmits normally again within ten seconds after you leave. The trainer runs at the keyer's speed (`wpm`), so changing it on a trainer page changes it everywhere. Its effective speed (`trainEff`, Farnsworth: characters at full speed, more space between them, 12 wpm by default, 0 = off) and its pitch (`trainTone`, 0 = the sidetone's) are its own.

| Exercise | What it does |
| --- | --- |
| **Listen and repeat** | SmartCW plays a character or group, you key it back on the paddles. The Koch method: it starts with two characters and adds one each time you get 90 % right. |
| **Copy characters** | Koch groups to copy by ear and type on the keyboard. |
| **Your callsign** | Hear your own call faster and faster, or key it cleanly. |
| **Callsigns** | Random calls built from real prefixes; the speed goes up as you get them. |
| **Words and abbreviations** | Common words, ham abbreviations and Q-codes, as whole words. |
| **Your own text** | Paste calls, words or a text, and copy it or key it. |
| **Q-codes and prosigns** | What QRL, QSB, `<KN>` and friends mean, with a reference list. |
| **QSO practice** | Answer a CQ, call CQ, a contest exchange and a park activation, over by over. |
| **Head copy** | Copy a complete QSO without writing, and get a score. |
| **Fist analyzer** | How even are your dits, dahs and spaces, what SmartCW has learned of your fist, and how surely it read each letter. |
| **Progress** | Accuracy per character in Koch order and your Koch level, with export and import. |

Progress is kept in the browser, not in SmartCW. Export it from the Progress page before you change computers.

## The fist analyzer

Key anything. The page shows every mark and space as a timeline, your average dit and dah with their spread, the dah ratio and the gaps inside and between letters, and tips: dahs too short or long, dits that vary, letters that run together.

Below it, **What SmartCW decodes you by**: the dit, dah, letter-inner gap and letter gap it has learned from you, updated with every element. **Save as my fist** keeps them in `config.txt`, so SmartCW starts from your fist at power-up; **Forget** goes back to the keyer speed.

**How sure it was** shows the last sixty letters it read, each coloured: green sat squarely in your fist, gold was fair, orange came close to the line between a dit and a dah or between a letter's inside gap and the gap between letters, red sat on or over it. Hover a letter for the number. Red and orange letters are the ones to key more evenly, or SmartCW is still settling on a new hand: it takes a few letters.

An iambic keyer makes perfect marks, so with paddles only your spacing shows. A straight key or a bug shows your real fist.
