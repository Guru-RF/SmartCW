#include "ptt.h"
#include "config.h"
#include "engine.h"
#include "hub.h"
#include "pins.h"
#include "wk.h"

static const unsigned long DEBOUNCE_MS = 30;
static const unsigned long LONG_PRESS_MS = 1500;

static unsigned long holdOffUntil = 0;
static bool wasPressed = false;
static bool longFired = false;
static unsigned long pressedSince = 0;
static int lastPaddleMode = KM_IAMBIC_B;
static bool cmdUsed = false; // paddles changed the speed during this press

void pttBegin() { pinMode(PIN_PTT, INPUT_PULLUP); }

void pttToggleKeyer() {
  if (config.keyer == KM_STRAIGHT) {
    config.keyer = lastPaddleMode;
  } else {
    lastPaddleMode = config.keyer;
    config.keyer = KM_STRAIGHT;
  }
  configMarkDirty();
  bool straight = config.keyer == KM_STRAIGHT;
  hubStream("!keyer ");
  hubStream(cfgEnumName(cfgFind("keyer"), config.keyer));
  hubStream("\n");
  engineBeep(straight ? 2 : 1);
  hubStateChanged();
}

void pttCommand(char c) {
  cmdUsed = true;
  int w = constrain(config.wpm + (c == '-' ? 1 : -1), 5, 99);
  if (w == config.wpm)
    return;
  config.wpm = w;
  if (wkOpen())
    wkSpeedChanged(); // the logger follows it like a speed pot
  else
    configMarkDirty();
  hubStateChanged();
}

void pttTick() {
  unsigned long now = millis();
  bool pressed = digitalRead(PIN_PTT) == LOW;
  engineCommandHold(pressed);
  if (pressed != wasPressed)
    wkButton(pressed);
  if (pressed && !wasPressed) {
    pressedSince = now;
    longFired = false;
    cmdUsed = false;
  }
  if (pressed && !longFired && !cmdUsed && now - pressedSince >= LONG_PRESS_MS) {
    longFired = true;
    pttToggleKeyer();
  }
  if (!pressed && wasPressed) {
    unsigned long held = now - pressedSince;
    if (cmdUsed) { // say the new speed on the sidetone, nothing is transmitted
      char b[24];
      snprintf(b, sizeof(b), "!wpm %d\n", config.wpm);
      hubStream(b);
      snprintf(b, sizeof(b), "%d", config.wpm);
      engineQueue(b, PROF_PRACTICE);
    } else if (!longFired && held >= DEBOUNCE_MS && (long)(now - holdOffUntil) >= 0) {
      hubPttPulse();
      holdOffUntil = now + 900;
    }
  }
  wasPressed = pressed;
}
