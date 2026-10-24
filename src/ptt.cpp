#include "ptt.h"
#include "config.h"
#include "engine.h"
#include "hub.h"
#include "pins.h"

static const unsigned long DEBOUNCE_MS = 30;
static const unsigned long LONG_PRESS_MS = 1500;

static unsigned long holdOffUntil = 0;
static bool wasPressed = false;
static bool longFired = false;
static unsigned long pressedSince = 0;
static int lastPaddleMode = KM_IAMBIC_B;

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

void pttTick() {
  unsigned long now = millis();
  bool pressed = digitalRead(PIN_PTT) == LOW;
  if (pressed && !wasPressed) {
    pressedSince = now;
    longFired = false;
  }
  if (pressed && !longFired && now - pressedSince >= LONG_PRESS_MS) {
    longFired = true;
    pttToggleKeyer();
  }
  if (!pressed && wasPressed) {
    unsigned long held = now - pressedSince;
    if (!longFired && held >= DEBOUNCE_MS && (long)(now - holdOffUntil) >= 0) {
      hubPttPulse();
      holdOffUntil = now + 900;
    }
  }
  wasPressed = pressed;
}
