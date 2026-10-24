#include "leds.h"
#include "pins.h"

// LEDs are active-low.
static void apply(int pin, bool on) { digitalWrite(pin, on ? LOW : HIGH); }

void ledsBegin() {
  const int pins[] = {PIN_LED_PWR, PIN_LED_DAH, PIN_LED_DIT, PIN_LED_SER, PIN_LED_SER2};
  for (int p : pins) {
    pinMode(p, OUTPUT);
    apply(p, false);
  }
}

void ledPwr(bool on) { apply(PIN_LED_PWR, on); }
void ledDit(bool on) { apply(PIN_LED_DIT, on); }
void ledDah(bool on) { apply(PIN_LED_DAH, on); }
void ledSer(bool on) { apply(PIN_LED_SER, on); }
void ledSer2(bool on) { apply(PIN_LED_SER2, on); }

// PTT flash: {pwr, ser2} every 150 ms, mirrors the original sequence.
static const uint8_t pttSteps[][2] = {{0, 1}, {0, 1}, {1, 1}, {0, 1}, {0, 1}, {0, 0}, {1, 0}};
static const int pttStepCount = sizeof(pttSteps) / sizeof(pttSteps[0]);
static int pttStep = -1;
static unsigned long pttLast = 0;

void ledsPttFlash() {
  pttStep = 0;
  pttLast = millis();
  ledPwr(pttSteps[0][0]);
  ledSer2(pttSteps[0][1]);
}

void ledsTick() {
  if (pttStep < 0)
    return;
  unsigned long now = millis();
  if (now - pttLast < 150)
    return;
  pttLast = now;
  pttStep++;
  if (pttStep >= pttStepCount) {
    pttStep = -1;
    ledPwr(true);
    ledSer2(false);
    return;
  }
  ledPwr(pttSteps[pttStep][0]);
  ledSer2(pttSteps[pttStep][1]);
}
