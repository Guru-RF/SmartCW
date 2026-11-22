# 8. Updates and troubleshooting

## Updating the firmware

Settings are kept across updates, and so is the serial number.

**From the website.** Open [smartcw.app/update](https://smartcw.app/update) in Chrome, Edge or Brave on a desktop or laptop, with SmartCW plugged in; Firefox and Safari cannot reach it. You choose SmartCW in the browser's list of serial ports, and the page checks its serial number with RF.Guru's bench records. It then restarts SmartCW into its update mode and asks you to pick *RP2 Boot* in the browser's USB list (once: after that the browser knows it), writes the newest release over the cable, reads it back and waits for SmartCW to come back with the new version. On Linux the browser needs one udev rule to reach RP2 Boot; the page shows it. On Windows, which has no driver of its own for RP2 Boot, the page uses the drive instead: SmartCW appears as a drive called `RPI-RP2`, you choose it in the folder dialog, and the page copies the firmware and watches the drive disappear. The page works for every SmartCW made on the RF.Guru bench, from firmware 2.3.0 on.

**By hand.** Download `smartcw-vX.Y.Z.uf2` from the [releases](https://github.com/Guru-RF/SmartCW/releases). Put SmartCW into update mode, then drag the file onto the `RPI-RP2` drive; the drive disappears and SmartCW restarts with the new firmware. Ways into update mode:

- System page → *Restart into update mode*.
- `!bootloader` on the serial port or TCP.
- Hold both paddles while plugging in, replace the contents of `config.txt` on the `SMARTCW` drive with the single word `firmwareupdate`, eject.
- **Recovery**: unplug, hold the BOOTSEL button, plug in, release. This works whatever the firmware does.

`flash_nuke.uf2`, shipped with every release, erases the whole flash: `config.txt`, and the RF.Guru serial number too. Flash the regular firmware after it. Without its serial number smartcw.app/update no longer takes the unit: [RF.Guru support](https://shop.rf.guru/pages/contact) sends a one-time link that records it again.

## Update mode, and how to tell

In update mode SmartCW runs no firmware: **all three LEDs are lit, steadily**, there is no serial port and no web page, and the computer sees a drive called `RPI-RP2` (the browser's USB list calls it *RP2 Boot*). Dragging a firmware file onto it (or the website writing it) ends that state. If the LEDs stay all on after a copy, the file never made it: copy it again and wait for the drive to disappear before unplugging.

## Boot modes

What is pressed while the USB cable goes in selects the session:

| Held while plugging in | Result |
| --- | --- |
| nothing | normal: serial port, MIDI and network as set in `config.txt` |
| both paddles | normal, plus a USB drive called `SMARTCW` with `config.txt` |
| PTT button | normal, plus a USB keyboard for this session |
| BOOTSEL | update mode |

**Editing `config.txt` on the drive.** Edit, save and **eject** with the file manager's Eject: SmartCW reboots and loads the file. A plain `umount` on Linux does not send the eject; send `!reboot` or re-plug instead. While the drive is shown SmartCW does not save settings itself, and the file wins at the next start.

## Factory settings

System → *Factory reset*, or `!defaults`, puts every setting back and keeps your callsign, name, memories, beacon text, serial number counter and saved fist. `!defaults all` clears those too. The RF.Guru serial number survives both. Deleting `config.txt` on the drive and ejecting regenerates the defaults too.

## Common problems

**smartcw.local does not open.** Use http://192.168.7.1/. If that does not open either: check that `network` is on (it is by default), and on Windows 10 that the firmware is 2.3.4 or newer; an older unit is updated from a Mac, a Linux machine or a Raspberry Pi with Chrome or Edge, after which Windows 10 works too.

**The radio keys all the time, or never.** Check the output mode: *Key line* for a key input, *Audio + PTT* for a microphone or data input, and that the radio is set for a straight key, not for a paddle. Then check the Radio Interface: the PTT/CW jack goes to the radio's key input in *Key line* mode and to its PTT in *Audio + PTT* mode; AUDIO carries the tone.

**No sound.** `buzzer` on and `volume` up for the piezo; `lineLevel` for line out. In audio mode line out carries the transmit tone, and the sidetone only with `lineSidetone` on. The browser's sidetone needs one click on the page first.

**The paddles are swapped.** `paddleSwap`.

**The logger does not find a WinKeyer.** Pick SmartCW's serial port, 1200 baud, and make sure `winkeyer` is on. Close any other program that has the port open.

**The keyboard types the wrong symbols.** The USB keyboard is US layout; letters and digits are always right.

**Letters run together or break up when I key by hand.** The decoder learns your fist as you key; after a change of hand it needs a few letters. Open the trainer's Fist page to see what it has learned and which letters it was unsure about, and save your fist once it matches you.

**Everything is dead: all LEDs on.** SmartCW is in update mode without firmware: drag the firmware file onto the `RPI-RP2` drive, as under *By hand* above. smartcw.app/update needs SmartCW running to know it, so it cannot help here; for a unit that arrived like this, [RF.Guru support](https://shop.rf.guru/pages/contact) sends a one-time link that puts the firmware on.

**Is it the hardware?** `!inputs` on the System console shows the raw paddle and PTT contacts; `!selftest` lights each LED in turn and beeps.

## Reporting a problem

Open a [bug report](https://github.com/Guru-RF/SmartCW/issues/new?template=bug.yml) with the firmware version and serial number (System page, or `!version` and `!id`), the computer and software involved, the key and the radio with the output mode, what you did and what happened, what the LEDs did, and your `config.txt` or `!status` with the callsign blanked out. A feature request has [its own form](https://github.com/Guru-RF/SmartCW/issues/new?template=feature.yml). A unit that arrived with a part of the kit missing is for [RF.Guru support](https://shop.rf.guru/pages/contact), not an issue.
