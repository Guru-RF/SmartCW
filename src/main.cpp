// SmartCW - iambic keyer, CW trainer and CW interface for SDR software
// Copyright (c) 2023-2026 Joeri Van Dooren (ON6URE) - RF.Guru, MIT license
//
// Boot modes (read while plugging in):
//   both paddles held -> config.txt exposed as USB drive "SMARTCW"
//   PTT held          -> decoded CW is typed as USB keystrokes
//
// Timing-critical work (paddles, keyer, text player, sidetone, key line) runs
// in a 1 kHz timer interrupt (engine.cpp). The loop below only moves events to
// USB MIDI, the serial port, the web interface and the TCP port.
#include "cli.h"
#include "config.h"
#include "crash.h"
#include "cwd.h"
#include "engine.h"
#include "hub.h"
#include "ident.h"
#include "leds.h"
#include "net.h"
#include "pins.h"
#include "ptt.h"
#include "usbio.h"
#include "wk.h"
#include <Arduino.h>

void setup() {
  crashBegin();
  identBegin(); // the USB serial number, before Serial.begin() enumerates
  pinMode(PIN_DIT, INPUT_PULLUP);
  pinMode(PIN_DAH, INPUT_PULLUP);
  pinMode(PIN_PTT, INPUT_PULLUP);
  delay(10);
  bool driveMode = digitalRead(PIN_DIT) == LOW && digitalRead(PIN_DAH) == LOW;
  bool keyboardMode = digitalRead(PIN_PTT) == LOW;

  ledsBegin();
  Serial.begin(115200);

  bool fsOk = configBegin();
  // A logger's WinKeyer settings and a cwdaemon "+/-" speed step are
  // temporary: config.txt keeps the operator's own.
  configSetHook = wkUserSet;
  configSaveFilter = [](Config &c) {
    wkPersistView(c);
    cwdPersistView(c);
  };
  engineBegin(); // the keyer works from here on, USB or not
  keyboardMode = keyboardMode || config.keyboard;

  // Register every USB interface back to back, before the host has finished
  // enumerating the bare serial port.
  usbBegin(config.midi, keyboardMode, driveMode && fsOk);
  netBegin();

  pttBegin();
  cliBegin();
  ledPwr(true);
  rp2040.wdt_begin(2000);

  // Give the host a moment to open the serial port before the banner.
  for (int i = 0; i < 15; i++) {
    rp2040.wdt_reset();
    delay(100);
  }
  Serial.println();
  Serial.print(F("!SmartCW " SMARTCW_VERSION " ready - keyer "));
  Serial.print(cfgEnumName(cfgFind("keyer"), config.keyer));
  Serial.print(F(", wpm "));
  Serial.print(config.wpm);
  Serial.print(F(", tone "));
  Serial.print(config.tone);
  Serial.print(F(" Hz, output "));
  Serial.print(cfgEnumName(cfgFind("output"), config.output));
  Serial.print(F(", midi "));
  Serial.print(usbMidiActive() ? F("on") : F("off"));
  Serial.print(F(", keyboard "));
  Serial.print(usbKeyboardActive() ? F("on") : F("off"));
  Serial.print(F(", drive "));
  Serial.println(usbDriveActive() ? F("on") : F("off"));
  netPrintStatus(Serial);
  crashReport(Serial);
  Serial.println(F("!type text to send CW, !help for commands"));
}

// Longest time spent in each part of the loop since the last "!perf".
uint32_t perfMax[6];
static inline void timed(int i, uint32_t &t) {
  uint32_t n = time_us_32();
  if (n - t > perfMax[i])
    perfMax[i] = n - t;
  t = n;
}

void loop() {
  uint32_t t = time_us_32(), t0 = t;
  rp2040.wdt_reset();
  ledsTick();
  hubTick();
  timed(0, t);
  pttTick();
  cliTick();
  wkTick();
  timed(1, t);
  netTick();
  timed(2, t);
  configTick(!engineBusy(), usbDriveActive());
  timed(3, t);
  if (t - t0 > perfMax[4])
    perfMax[4] = t - t0;
}
