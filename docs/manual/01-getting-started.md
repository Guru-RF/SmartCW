# 1. Getting started

## In the box

- The SmartCW keyer.
- A USB-C to USB-A cable, 1 m.
- The breakout for CW, audio and PTT, with its cable: the part that connects SmartCW to your radio (see [chapter 3](03-radio.md)).

The firmware is already on the unit, with an RF.Guru serial number (`SCW-xxxxx`). Updates come from [smartcw.app](https://smartcw.app), over the same USB cable.

## The box itself

| Where | What |
| --- | --- |
| Back | USB-C: power, serial port, MIDI, network and keyboard, all over one cable |
| Top | The PWR, DAH and DIT LEDs and the **PTT** button |
| Front | The **PADDLE** jack and the **CW** jack; the CW jack also carries line out, for the sidetone or the transmit tone |
| Inside | A piezo for the sidetone |

The PADDLE jack takes paddles (dit on the tip, dah on the ring) or a straight key (tip, ring or both: a mono plug works). The CW jack goes to the radio, through the breakout.

| LED | Meaning |
| --- | --- |
| PWR | on while SmartCW runs; off while it sends text, a memory or the beacon; blinks on a press of the PTT button |
| DIT | on while a dit is keyed, or the straight key is closed |
| DAH | on while a dah is keyed |

All three LEDs on at once, steadily, means SmartCW sits in its update mode (the bootloader) and runs no firmware: see [chapter 8](08-updates-and-troubleshooting.md).

## First ten minutes

1. **Plug in the paddles.** Dit on the tip, dah on the ring. Left-handed: swap them later in Settings with *paddleSwap*.
2. **Plug the USB cable into the computer.** SmartCW starts within a second: PWR lights, the sidetone is live. Linux, macOS and Windows 10/11 need no driver: a serial port, a MIDI port and a small network adapter appear by themselves.
3. **Key something.** The piezo sounds your dits and dahs at 18 words per minute, the DIT and DAH LEDs follow. Nothing goes to a radio yet: the CW jack is empty.
4. **Open the web page.** In any browser go to **http://smartcw.local/** or **http://192.168.7.1/**. The Live page shows the Morse tree following your paddles and the text SmartCW decodes.
5. **Set yourself up.** On the Settings page enter your callsign and name (the memories use them), pick your key type and speed, and set the sidetone pitch. Every change applies at once and is saved a moment later.
6. **Connect the radio** when you are ready: [chapter 3](03-radio.md).

## Speed without a computer

Hold the PTT button and tap the dit paddle for 1 wpm slower, the dah paddle for 1 wpm faster. When you let go, SmartCW says the new speed on the sidetone. It is saved.

## Where things are set

Every setting lives in one plain text file, `config.txt`, inside SmartCW. You change it from the web page, with a `!` command on the serial port or TCP, or by editing the file on the `SMARTCW` USB drive (hold both paddles while plugging in). [Chapter 7](07-reference.md) lists every setting.
