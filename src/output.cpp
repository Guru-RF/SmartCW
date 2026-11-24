#include "output.h"
#include "config.h"
#include "leds.h"
#include "pins.h"
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>

static const uint16_t WRAP = 4095;

// Line out is a sine, not a square. Its PWM runs flat out at about 65 kHz
// (wrap 2047, no divider), far above hearing, and the slice's wrap interrupt
// writes one sample of a sine per period. lineLevel sets the amplitude, 4 dB
// a step, so it is a volume at last: a square wave's duty cycle, which it
// used to set, changes the timbre and barely the loudness, and 3.3 V peak
// into headphones is painful at every setting. Idle sits at the midpoint and
// the tone fades in and out over 4 ms, so keying does not click.
static const uint16_t LINE_WRAP = 2047;
static const uint16_t LINE_MID = (LINE_WRAP + 1) / 2;
static const uint LINE_SLICE = 7, LINE_CHAN = PWM_CHAN_B; // GPIO 15
static int16_t sineTab[256];
static const uint16_t lineAmpTab[11] = {0, 16, 26, 41, 65, 102, 162, 257, 407, 645, 1023}; // 1023 * 10^(-(10-level)*4/20)
static volatile uint32_t linePhaseInc = 0;
static volatile uint16_t lineAmp = 0;
static volatile bool lineOn = false;
static uint32_t linePhase = 0;
static uint16_t lineEnv = 0; // 0..256, the fade
static volatile bool cfgChanged = true;
static volatile bool forceBuzzer = false;
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

static void __not_in_flash_func(lineIsr)() {
  if (!(pwm_get_irq_status_mask() & (1u << LINE_SLICE)))
    return;
  pwm_clear_irq(LINE_SLICE);
  if (lineOn) {
    if (lineEnv < 256)
      lineEnv++;
  } else if (lineEnv) {
    lineEnv--;
  }
  uint16_t v = LINE_MID;
  if (lineEnv) {
    linePhase += linePhaseInc;
    int32_t a = ((int32_t)lineAmp * lineEnv) >> 8;
    v = (uint16_t)(LINE_MID + ((sineTab[linePhase >> 24] * a) >> 15));
  }
  pwm_set_chan_level(LINE_SLICE, LINE_CHAN, v);
}

static void lineInit() {
  for (int i = 0; i < 256; i++)
    sineTab[i] = (int16_t)lroundf(32767.0f * sinf(i * 6.2831853f / 256));
  gpio_set_function(PIN_LINEOUT, GPIO_FUNC_PWM);
  pwm_set_wrap(LINE_SLICE, LINE_WRAP);
  pwm_set_clkdiv_int_frac(LINE_SLICE, 1, 0);
  pwm_set_chan_level(LINE_SLICE, LINE_CHAN, LINE_MID);
  pwm_clear_irq(LINE_SLICE);
  pwm_set_irq_enabled(LINE_SLICE, true);
  irq_add_shared_handler(PWM_IRQ_WRAP, lineIsr, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);
  irq_set_priority(PWM_IRQ_WRAP, 0); // short, and it must not miss a sample under the engine tick
  irq_set_enabled(PWM_IRQ_WRAP, true);
  pwm_set_enabled(LINE_SLICE, true);
}

static void lineTone(int hz) {
  uint32_t fs = clock_get_hz(clk_sys) / (LINE_WRAP + 1);
  linePhaseInc = (uint32_t)(((uint64_t)hz << 32) / fs);
}

static void lineSet(bool on, int level) {
  if (level < 1)
    level = 1;
  if (level > 10)
    level = 10;
  lineAmp = lineAmpTab[level];
  lineOn = on;
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
  setLevel(PIN_BUZZER, sideOn && (config.buzzer || forceBuzzer), config.volume);
  // Key and practice modes: line out is a second sidetone. Audio mode: it is
  // the radio's input and carries the TX tone after the PTT lead, plus the
  // sidetone when the operator asked for it there too (lineSidetone).
  bool line = (curOutput == OUT_AUDIO) ? (lineTx || (config.lineSidetone && sideOn)) : sideOn;
  lineSet(line, config.lineLevel);
}

void outBegin() {
  pinMode(PIN_CWOUT, OUTPUT);
  digitalWrite(PIN_CWOUT, LOW);
  pwmInit(PIN_BUZZER);
  lineInit();
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
  lineTone(hz);
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
void outForceBuzzer(bool on) { forceBuzzer = on; }
bool outKeyIsDown() { return keyDown; }
