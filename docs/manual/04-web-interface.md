# 4. The web interface

SmartCW is also a small network adapter over its USB cable, with its own web page at **http://smartcw.local/** or **http://192.168.7.1/**. It gives your computer an address next to its own (192.168.7.16 and up) and hands out no gateway and no DNS server, so your internet connection is not touched. There is nothing to install and no internet needed; the page lives in SmartCW.

The name `smartcw.local` is answered by SmartCW itself, over mDNS, on Linux (Avahi), macOS (Bonjour) and Windows. If the name does not resolve on your computer, the address always works.

## Live

The operating page.

- **The Morse tree** follows every dit and dah from your paddles down the classic tree. Tap a letter to hear it; swipe or use the dots under it for numbers and signs. The gear chooses whether the tree sounds on SmartCW or in the browser.
- **The decoded text** with its Morse underneath, a speaker to hear it in the browser, a replay button and a clear button.
- **An on-screen key**: tap for a dot, hold for a dash, with space and backspace next to it. It keys SmartCW like a paddle would.
- **Key type and output mode**, **speed** and **tone**, changed right there.
- **Text sending**: type, Enter sends; `<SK>`, `<KN>`, `<AR>`, `<BT>` for prosigns. **Send**, **Stop** and **PTT**.
- **Memories** as buttons, the serial number field, **Tune 5 s** and **Beacon**.
- **The monitor**: everything sent and received, in order.

## Settings

Every setting SmartCW has, grouped in cards: Keyer, Sidetone, Transmit output, Text sending, Trainer, Station (callsign and name), Memories, Beacon, USB MIDI, Computer (keyboard, internet CW and contest loggers) and USB network. A change applies at once and is written to `config.txt` a moment later. Settings that change the USB layout (MIDI, keyboard, network, the address and ports) say *restart* and apply after one.

**Reset to defaults** puts every setting back and keeps your callsign, name and memories.

**Sidetone in this browser** plays what you key on the computer too, a moment after SmartCW, with its own **Browser volume**. Both are preferences of that browser, not settings in `config.txt`. A browser only plays sound after you have clicked on the page once.

## System

- **Status**: firmware version, the RF.Guru serial number, uptime, what USB is presenting, the network address and the connected clients. A recorded crash shows here too.
- **Console**: any `!` command, with the common ones as buttons. [Chapter 7](07-reference.md) lists them.
- **Backup and restore** of `config.txt`: download it, or restore it from a file.
- **Firmware update**: *Restart into update mode* (see [chapter 8](08-updates-and-troubleshooting.md)).
- **Restart** (applies USB and network changes) and **Factory reset**.
- **Connecting software**: the MIDI note table and how to set up SDR software, loggers and internet CW, with SmartCW's own values filled in.

## Trainer

Its own chapter: [5. The trainer](05-trainer.md).

## The TCP port

The same text and command protocol as the serial port, on port 23: `nc 192.168.7.1 23` or `telnet smartcw.local`. SmartCW also announces itself with DNS-SD, so service browsers list it as a web site and a telnet service.
