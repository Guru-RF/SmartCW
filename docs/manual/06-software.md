# 6. Loggers and software

One USB cable gives the computer a serial port, a MIDI port, a network adapter and, when turned on, a keyboard. Each is a way for software to work with SmartCW.

## Contest loggers: WinKeyer

SmartCW's USB serial port speaks the K1EL WinKeyer 2 protocol. In a logger with WinKeyer support (N1MM+, DXLog, Win-Test, fldigi, CQRLOG, Log4OM, UcxLog, RUMlogNG, MacLoggerDX and others), choose SmartCW's serial port (`COMx` on Windows, `/dev/ttyACM0` on Linux, `/dev/tty.usbmodem…` on macOS) as the WinKeyer, at the usual 1200 baud. The logger's first WinKeyer command switches the port over; when the logger closes it, the port is SmartCW's own text protocol again.

While the logger has it open:

- The logger's speed, weight, dah ratio, tone, key mode, PTT lead and tail, keying compensation and first extension are used. SmartCW's own settings are left alone and come back when the logger closes. A straight key stays a straight key.
- The paddles break in as on a real WinKeyer: they stop the logger's message and clear its buffer, and what you key is echoed to the logger.
- PTT + paddle speed changes are reported as the WinKeyer speed pot; the PTT button as pushbutton 1.
- The web page shows a blue bar. The web page, MIDI, TCP and the trainer keep working next to the logger.

Set `winkeyer` off if another program on the serial port gets confused by the WinKeyer answers.

## Linux loggers: cwdaemon

Loggers that key through cwdaemon (CQRLOG, TLF, xlog, KLog) can send straight to SmartCW over the USB network: set the cwdaemon host to `192.168.7.1` or `smartcw.local` and the port to `6789`. Speed, tone, weight, tune, abort and the reply requests are supported. `cwdaemonPort` changes the port, 0 turns it off.

## SDR software: MIDI

SmartCW is a USB MIDI device named *SmartCW*, channel 1. Map its notes in the SDR program (SDR Console, Thetis, PowerSDR, SparkSDR and others key CW from MIDI):

| Event | Note (default) |
| --- | --- |
| key down / up, paddles or text (`midiMode: keyed`) | `midiCwNote`, 65 |
| straight key closed / open | `midiStraightNote`, 64 |
| dit / dah contact (`midiMode: paddles`) | `midiDitNote` 67 / `midiDahNote` 68 |
| PTT button | `midiPttNote`, 66, on then off 150 ms later |
| audio-mode PTT | `midiPttNote` held |

*Keyed* mode sends the keyer's finished output as one note; *paddles* mode sends the raw contacts as two notes for software that does its own keying. The System page shows the table with your values. Nothing is sent over MIDI in practice mode or while a trainer page is open. `midi: off` removes the MIDI port (restart).

## Internet CW: vBand and Vail

vBand and Vail listen for the Left and Right Ctrl keys. In Settings → Computer, turn **USB keyboard** on and set **Keyboard sends** to *Paddle keys* (`keyboard: true`, `keyboardMode: ctrl`), restart once. The dit paddle then holds Left Ctrl, the dah paddle Right Ctrl, a straight key Left Ctrl; the website does the keying. Choose the keyer in vBand or Vail, *Straight key* for a straight key. SmartCW's own sidetone keeps running: turn `buzzer` off or set the output to practice if you only want the website's sound.

Vail also works over MIDI: press *Vail notes* in Settings → USB MIDI (key and straight key = note 0, dit = 1, dah = 2). In keyed mode pick *Straight key* as Vail's keyer; in paddles mode Vail does the keying.

## Typing what you key: keyboard mode

With `keyboard: true` and `keyboardMode: text` (or for one session, hold the PTT button while plugging in) SmartCW is also a USB keyboard and types every character it decodes from your paddles or key into whatever window has focus: a chat, a log, a text editor. A word gap types a space, a short press on PTT types Enter. Unknown patterns are not typed. The layout is US: on a computer with another layout some punctuation may come out differently.

## Your own programs: serial and TCP

The serial port (any baud rate) and TCP port 23 speak the same simple protocol: any character you send is keyed and echoed as it goes out, whatever you key comes back as text, and a line starting with `!` is a command whose answer starts with `!`. [Chapter 7](07-reference.md) lists the commands. There is also an HTTP API and a live event stream on port 80, open to pages on other origins; they are described in [INTEROP.md](../../INTEROP.md).
