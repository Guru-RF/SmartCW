#include "output.h"
#include "config.h"
#include "leds.h"
#include "pins.h"
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

static const uint16_t WRAP = 4095;
static volatile bool cfgChanged = true;
static int curTone = 0;
static int curOutput = OUT_KEY;

static bool sideOn = false;  // sidetone sounding (key or beep)
static bool keyDown = false;
static bool markTx = false;  // the current mark goes out on the TX path
static bool pttOn = false;
static bool lineTx = false;  // audio mode: line out tone after the lead delay
static uint32_t lastTxUp = 0;

struct Transition {
  uint32_t t;
  bool on;
};
static const uint8_t RING = 64;
static Transition ring[RING];
static uint8_t rHead = 0, rTail = 0;

static void pwmInit(int pin) {
  gpio_set_function(pin, GPIO_FUNC_PWM);
  uint slice = pwm_gpio_to_slice_num(pin);
  pwm_set_wrap(slice, WRAP);
  pwm_set_chan_level(slice, pwm_gpio_to_channel(pin), 0);
  pwm_set_enabled(slice, true);
}

static void pwmFreq(int pin, int hz) {
  uint slice = pwm_gpio_to_slice_num(pin);
  uint32_t sys = clock_get_hz(clk_sys);
  // clkdiv in 8.4 fixed point
  uint32_t div16 = (uint32_t)(((uint64_t)sys * 16) / ((uint64_t)hz * (WRAP + 1)));
  if (div16 < 16)
    div16 = 16;
  if (div16 > 255 * 16 + 15)
    div16 = 255 * 16 + 15;
  pwm_set_clkdiv_int_frac(slice, div16 >> 4, div16 & 15);
}

static void setLevel(int pin, bool on, int level) {
  if (level < 1)
    level = 1;
  if (level > 10)
    level = 10;
  uint16_t v = on ? (uint16_t)((WRAP + 1) / 2 * level / 10) : 0;
  pwm_set_chan_level(pwm_gpio_to_slice_num(pin), pwm_gpio_to_channel(pin), v);
}

static void applySound() {
  setLevel(PIN_BUZZER, sideOn && config.buzzer, config.volume);
  bool line = (curOutput == OUT_AUDIO) ? lineTx : sideOn;
  setLevel(PIN_LINEOUT, line, config.lineLevel);
}

void outBegin() {
  pinMode(PIN_CWOUT, OUTPUT);
  digitalWrite(PIN_CWOUT, LOW);
  pwmInit(PIN_BUZZER);
  pwmInit(PIN_LINEOUT);
  curOutput = config.output;
  outSetTone(config.tone);
}

void outApplyConfig() { cfgChanged = true; }

void outSetTone(int hz) {
  if (hz < 100)
    hz = 100;
  if (hz == curTone)
    return;
  curTone = hz;
  pwmFreq(PIN_BUZZER, hz);
  pwmFreq(PIN_LINEOUT, hz);
}

void outAllOff() {
  gpio_put(PIN_CWOUT, 0);
  pttOn = false;
  markTx = false;
  lineTx = false;
  sideOn = false;
  keyDown = false;
  rHead = rTail = 0;
  ledSer2(false);
  applySound();
}

static void pushTransition(uint32_t now, bool on) {
  uint8_t n = (uint8_t)((rHead + 1) % RING);
  if (n == rTail) { // full: drop the oldest
    lineTx = ring[rTail].on;
    rTail = (uint8_t)((rTail + 1) % RING);
  }
  ring[rHead].t = now;
  ring[rHead].on = on;
  rHead = n;
}

void outKey(bool down, bool tx, uint32_t now) {
  if (down == keyDown)
    return;
  keyDown = down;
  sideOn = down;
  if (down)
    markTx = tx;
  if (markTx) {
    if (curOutput == OUT_KEY) {
      gpio_put(PIN_CWOUT, down);
    } else if (curOutput == OUT_AUDIO) {
      if (down && !pttOn) {
        pttOn = true;
        gpio_put(PIN_CWOUT, 1);
        ledSer2(true);
      }
      pushTransition(now, down);
      if (!down)
        lastTxUp = now;
    }
  }
  if (!down)
    markTx = false;
  applySound();
}

void outBeep(bool on) {
  if (keyDown)
    return;
  sideOn = on;
  applySound();
}

void outTick(uint32_t now) {
  if (cfgChanged) {
    cfgChanged = false;
    if (config.output != curOutput) {
      outAllOff();
      curOutput = config.output;
    }
    if (!keyDown)
      outSetTone(config.tone);
    applySound();
  }
  if (curOutput != OUT_AUDIO)
    return;
  bool changed = false;
  while (rTail != rHead && (int32_t)(now - ring[rTail].t) >= config.pttLead) {
    lineTx = ring[rTail].on;
    rTail = (uint8_t)((rTail + 1) % RING);
    changed = true;
  }
  if (changed)
    applySound();
  if (pttOn && !markTx && rTail == rHead &&
      (int32_t)(now - lastTxUp) >= config.pttLead + config.pttTail) {
    pttOn = false;
    gpio_put(PIN_CWOUT, 0);
    ledSer2(false);
  }
}

bool outPttActive() { return pttOn; }
bool outKeyIsDown() { return keyDown; }
