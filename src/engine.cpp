// SmartCW real-time engine, driven by a 1 kHz timer interrupt.
// Iambic B logic originally after xiaoKey, Copyright 2022 Mark Woodworth
// (AC9YW), MIT licensed: https://github.com/MarkWoodworth/xiaokey
#include "engine.h"
#include "config.h"
#include "leds.h"
#include "morse.h"
#include "output.h"
#include "pins.h"
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <pico/time.h>

// Run-together markers in the text queue for unknown "<XYZ>" prosigns.
static const char RUN_START = 1;
static const char RUN_END = 2;

// ---------------- event ring: interrupt -> loop ----------------
static const uint16_t EVQ = 256;
static Event evq[EVQ];
static volatile uint16_t evHead = 0, evTail = 0;
static volatile bool evLost = false;

static void push(uint8_t t, uint8_t a, uint16_t b = 0, uint16_t c = 0) {
  uint16_t h = evHead;
  uint16_t n = (uint16_t)((h + 1) % EVQ);
  if (n == evTail) {
    evLost = true;
    return;
  }
  evq[h].type = t;
  evq[h].a = a;
  evq[h].b = b;
  evq[h].c = c;
  __compiler_memory_barrier();
  evHead = n;
}

bool enginePoll(Event &e) {
  uint16_t t = evTail;
  if (t == evHead)
    return false;
  __compiler_memory_barrier();
  e = evq[t];
  __compiler_memory_barrier();
  evTail = (uint16_t)((t + 1) % EVQ);
  return true;
}

bool engineEventsLost() {
  bool l = evLost;
  evLost = false;
  return l;
}

// ---------------- text queue: loop -> interrupt ----------------
static const uint16_t TXQ = 1024;
static uint16_t txq[TXQ];
static volatile uint16_t tqHead = 0, tqTail = 0;

static bool tqPut(uint8_t profile, char c) {
  uint16_t h = tqHead;
  uint16_t n = (uint16_t)((h + 1) % TXQ);
  if (n == tqTail)
    return false;
  txq[h] = (uint16_t)((profile << 8) | (uint8_t)c);
  __compiler_memory_barrier();
  tqHead = n;
  return true;
}

size_t engineQueueFree() { return (size_t)((tqTail + TXQ - tqHead - 1) % TXQ); }

size_t engineQueue(const char *text, uint8_t profile) {
  size_t n = 0;
  for (const char *p = text; *p; p++) {
    char c = *p;
    if (c == '<') {
      const char *e = strchr(p, '>');
      if (e && e - p >= 2 && e - p <= 8) {
        char name[8] = {0};
        memcpy(name, p + 1, (size_t)(e - p - 1));
        char ps = (e - p - 1 == 2) ? morseProsign(name) : 0;
        if (ps) {
          if (!tqPut(profile, ps))
            break;
        } else { // unknown: the letters run together
          if (engineQueueFree() < (size_t)(e - p + 1))
            break;
          tqPut(profile, RUN_START);
          for (const char *q = p + 1; q < e; q++)
            tqPut(profile, *q);
          tqPut(profile, RUN_END);
        }
        n += (size_t)(e - p + 1);
        p = e;
        continue;
      }
    }
    if (c == '\r' || c == '\n' || c == '\t')
      c = ' ';
    if ((uint8_t)c < 0x20 || (uint8_t)c >= 0x7f)
      continue;
    if (!tqPut(profile, c))
      break;
    n++;
  }
  return n;
}

// ---------------- requests from the loop ----------------
static volatile bool stopReq = false, resetReq = false, tuneReq = false;
static volatile uint8_t beepReq = 0;
static volatile uint32_t tuneReqMs = 0;
static volatile uint32_t practiceUntil = 0;
static volatile int trainWpm = 0, trainEff = 0; // per-request trainer speed

// ---------------- timing ----------------
struct Timing {
  uint16_t dit, ditOn, dahOn, elGap, charGap, wordGap;
};

static void calcTiming(Timing &t, int wpm, int eff) {
  if (wpm < 5)
    wpm = 5;
  int32_t dit = 1200 / wpm;
  int32_t on = dit * config.weight / 50;
  if (on < 1)
    on = 1;
  int32_t dah = dit * config.ratio / 10 + (on - dit);
  if (dah <= on)
    dah = on + 1;
  int32_t gap = 2 * dit - on;
  if (gap < 1)
    gap = 1;
  t.dit = (uint16_t)dit;
  t.ditOn = (uint16_t)on;
  t.dahOn = (uint16_t)dah;
  t.elGap = (uint16_t)gap;
  if (eff > 0 && eff < wpm) {
    // ARRL Farnsworth: total spacing time per "PARIS " in ms
    int32_t ta = (60000 * wpm - 37200 * eff) / (wpm * eff);
    t.charGap = (uint16_t)(3 * ta / 19);
    t.wordGap = (uint16_t)(7 * ta / 19);
  } else {
    t.charGap = (uint16_t)(3 * dit);
    t.wordGap = (uint16_t)(7 * dit);
  }
}

// ---------------- paddles ----------------
static uint8_t histDit = 0, histDah = 0;
static bool padDit = false, padDah = false;

static void readPaddles(bool &d, bool &h, bool &dRise, bool &hRise, bool &dFall, bool &hFall) {
  bool rd = !gpio_get(PIN_DIT), rh = !gpio_get(PIN_DAH);
  if (config.paddleSwap) {
    bool t = rd;
    rd = rh;
    rh = t;
  }
  histDit = (uint8_t)((histDit << 1) | rd);
  histDah = (uint8_t)((histDah << 1) | rh);
  bool nd = padDit, nh = padDah;
  if ((histDit & 0x07) == 0x07)
    nd = true;
  else if ((histDit & 0x07) == 0)
    nd = false;
  if ((histDah & 0x07) == 0x07)
    nh = true;
  else if ((histDah & 0x07) == 0)
    nh = false;
  dRise = nd && !padDit;
  hRise = nh && !padDah;
  dFall = !nd && padDit;
  hFall = !nh && padDah;
  padDit = nd;
  padDah = nh;
  d = nd;
  h = nh;
}

// ---------------- marks ----------------
static uint16_t markFlags = 0;

static void markOn(uint16_t flags, uint32_t now) {
  outKey(true, (flags & EVK_TX) != 0, now);
  markFlags = flags;
  push(EV_KEY, 1, flags);
}

static void markOff(uint32_t now) {
  if (!outKeyIsDown())
    return;
  outKey(false, false, now);
  push(EV_KEY, 0, markFlags);
}

static void ledsOff() {
  ledDit(false);
  ledDah(false);
}

// ---------------- keyer + decoder ----------------
enum KState : uint8_t { K_IDLE, K_MARK, K_GAP, K_MANUAL };
static KState ks = K_IDLE;
static char curEl = 0, lastPress = '.';
static bool ditMem = false, dahMem = false;
static bool inChar = false, inWord = false;
static bool mDown = false; // straight key closed
static uint32_t kUntil = 0, kLastEnd = 0, kIdleSince = 0, kMarkStart = 0;
static uint16_t kGapBefore = 0;
static char pat[12];
static uint8_t patLen = 0;
static Timing kT;
static uint16_t adaptDit = 66;
static int lastMode = -1;

static uint16_t clip16(uint32_t v) { return v > 65535 ? 65535 : (uint16_t)v; }

static void addElement(char e, uint16_t markMs, uint16_t gapMs) {
  if (patLen < sizeof(pat) - 1) {
    pat[patLen++] = e;
    pat[patLen] = 0;
  }
  push(EV_ELEM, (uint8_t)e, markMs, gapMs);
}

static void emitChar() {
  if (patLen == 0)
    return;
  char c = morseDecode(pat);
  push(EV_RX, (uint8_t)c);
  patLen = 0;
  pat[0] = 0;
}

static void keyerClear(uint32_t now) {
  if (ks == K_MARK || ks == K_MANUAL || mDown)
    markOff(now);
  ledsOff();
  ks = K_IDLE;
  ditMem = dahMem = false;
  inChar = inWord = false;
  mDown = false;
  patLen = 0;
  pat[0] = 0;
  kIdleSince = kLastEnd = now;
  adaptDit = (uint16_t)(1200 / (config.wpm > 0 ? config.wpm : 18));
}

static char choose(bool d, bool h, bool fromIdle) {
  int m = config.keyer;
  if (m == KM_BUG)
    return ditMem ? '.' : 0;
  if (m == KM_ULTIMATIC) {
    if ((d && h) || (ditMem && dahMem))
      return lastPress; // the latest paddle wins
    return ditMem ? '.' : (dahMem ? '-' : 0);
  }
  if (ditMem && dahMem) {
    if (fromIdle)
      return lastPress == '.' ? '-' : '.'; // the earlier of the two first
    return curEl == '.' ? '-' : '.';       // squeeze: alternate
  }
  return ditMem ? '.' : (dahMem ? '-' : 0);
}

static void kStart(char e, uint32_t now, bool tx) {
  outSetTone(config.tone);
  uint16_t gap = clip16(now - kLastEnd);
  curEl = e;
  inChar = inWord = true;
  ditMem = dahMem = false;
  uint16_t on = (e == '.') ? kT.ditOn : kT.dahOn;
  markOn(tx ? EVK_TX : 0, now);
  if (e == '.')
    ledDit(true);
  else
    ledDah(true);
  addElement(e, on, gap);
  ks = K_MARK;
  kUntil = now + on;
}

static void manualPress(uint32_t now, uint16_t flags) {
  outSetTone(config.tone);
  kGapBefore = clip16(now - kLastEnd);
  markOn(flags, now);
  ledDit(true);
  kMarkStart = now;
  inChar = inWord = true;
}

static void manualRelease(uint32_t now, bool adaptive) {
  markOff(now);
  ledDit(false);
  uint32_t dur = now - kMarkStart;
  uint32_t ref = adaptive ? adaptDit : kT.dit;
  char e = dur < 2 * ref ? '.' : '-';
  if (adaptive) {
    uint32_t est = (e == '.') ? dur : dur / 3;
    uint32_t a = (3u * adaptDit + est) / 4u;
    if (a < 20)
      a = 20;
    if (a > 400)
      a = 400;
    adaptDit = (uint16_t)a;
  }
  addElement(e, clip16(dur), kGapBefore);
  kLastEnd = now;
  kIdleSince = now;
}

static void straightTick(uint32_t now, bool down, bool tx) {
  if (down != mDown) {
    mDown = down;
    if (down)
      manualPress(now, (uint16_t)(EVK_STRAIGHT | (tx ? EVK_TX : 0)));
    else
      manualRelease(now, true);
    return;
  }
  if (down)
    return;
  uint32_t idle = now - kLastEnd;
  if (inChar && idle * 2 > 5u * adaptDit) { // 2.5 dits
    inChar = false;
    emitChar();
  } else if (inWord && !inChar && idle > 6u * adaptDit) {
    inWord = false;
    push(EV_RX, ' ');
  }
}

static void autoTick(uint32_t now, bool d, bool h, bool tx) {
  int m = config.keyer;
  calcTiming(kT, config.wpm, 0);

  if (m == KM_BUG) { // dah paddle = manual key
    if (ks == K_MANUAL) {
      if (!h) {
        manualRelease(now, false);
        ks = K_IDLE;
      }
      return;
    }
    if (h && ks == K_IDLE) {
      manualPress(now, tx ? EVK_TX : 0);
      ks = K_MANUAL;
      return;
    }
  }

  if (d)
    ditMem = true;
  if (h && m != KM_BUG)
    dahMem = true;

  switch (ks) {
  case K_IDLE: {
    uint32_t idle = now - kIdleSince;
    if (inChar && idle > 2u * kT.dit) { // + the element gap = 3 dits
      inChar = false;
      emitChar();
    } else if (inWord && !inChar && idle > 6u * kT.dit) {
      inWord = false;
      push(EV_RX, ' ');
    }
    char next = choose(d, h, true);
    if (!next)
      break;
    if (config.autoSpace && inChar && m != KM_BUG)
      break; // finish the letter space first
    kStart(next, now, tx);
    break;
  }
  case K_MARK:
    if ((int32_t)(now - kUntil) >= 0) {
      markOff(now);
      ledsOff();
      kLastEnd = now;
      if (curEl == '.')
        ditMem = false; // re-latched during the gap while still held
      else
        dahMem = false;
      if (m == KM_IAMBIC_A) { // no squeeze memory: only what is held now
        if (curEl == '.')
          dahMem = dahMem && h;
        else
          ditMem = ditMem && d;
      }
      ks = K_GAP;
      kUntil = now + kT.elGap;
    }
    break;
  case K_GAP:
    if ((int32_t)(now - kUntil) >= 0) {
      char next = choose(d, h, false);
      if (next) {
        kStart(next, now, tx);
      } else {
        ks = K_IDLE;
        kIdleSince = now;
      }
    }
    break;
  default:
    break;
  }
}

// ---------------- text player ----------------
enum PPhase : uint8_t { P_IDLE, P_MARK, P_GAP, P_WAIT };
static PPhase pp = P_IDLE;
static const char *pPat = nullptr;
static uint8_t pIdx = 0, pProf = 0;
static uint32_t pUntil = 0;
static bool pRun = false, pActive = false;
static Timing pT;

static void playerClear(uint32_t now) {
  tqTail = tqHead;
  if (pp == P_MARK)
    markOff(now);
  if (pp != P_IDLE)
    ledsOff();
  pp = P_IDLE;
  pRun = false;
}

static void pStartMark(uint32_t now, bool practice) {
  bool tx = pProf == PROF_TX && !practice && config.output != OUT_OFF;
  char e = pPat[pIdx];
  markOn((uint16_t)(EVK_PLAYER | (tx ? EVK_TX : 0)), now);
  if (e == '.')
    ledDit(true);
  else
    ledDah(true);
  pp = P_MARK;
  pUntil = now + (e == '.' ? pT.ditOn : pT.dahOn);
}

static void playerTick(uint32_t now, bool practice) {
  if (pp == P_IDLE) {
    if (tqHead == tqTail) {
      if (pActive) {
        pActive = false;
        pRun = false;
        push(EV_DONE, 0);
        ledSer(false);
        ledPwr(true);
        outSetTone(config.tone);
      }
      return;
    }
    if (ks != K_IDLE || mDown || inChar)
      return; // the paddles have priority
    uint16_t t = tqTail;
    uint16_t v = txq[t];
    __compiler_memory_barrier();
    tqTail = (uint16_t)((t + 1) % TXQ);
    char c = (char)(v & 0xff);
    pProf = (uint8_t)(v >> 8);
    if (!pActive) {
      pActive = true;
      ledSer(true);
      ledPwr(false);
    }
    if (pProf == PROF_TRAIN) {
      if (trainWpm)
        calcTiming(pT, trainWpm, trainEff);
      else
        calcTiming(pT, config.trainWpm, config.trainEff);
      outSetTone(config.trainTone ? config.trainTone : config.tone);
    } else {
      calcTiming(pT, config.wpm, config.farnsworth);
      outSetTone(config.tone);
    }
    if (c == RUN_START) {
      pRun = true;
      return;
    }
    if (c == RUN_END) {
      pRun = false;
      pp = P_WAIT;
      pUntil = now + (pT.charGap > pT.elGap ? pT.charGap - pT.elGap : 0);
      return;
    }
    if (!pRun)
      push(EV_TX, (uint8_t)c, pProf);
    if (c == ' ') {
      pp = P_WAIT;
      pUntil = now + (pT.wordGap > pT.charGap ? pT.wordGap - pT.charGap : 0);
      return;
    }
    pPat = morseEncode(c);
    if (!pPat)
      return;
    pIdx = 0;
    pStartMark(now, practice);
    return;
  }
  if ((int32_t)(now - pUntil) < 0)
    return;
  switch (pp) {
  case P_MARK:
    markOff(now);
    ledsOff();
    pp = P_GAP;
    pUntil = now + pT.elGap;
    break;
  case P_GAP:
    pIdx++;
    if (pPat[pIdx]) {
      pStartMark(now, practice);
    } else if (pRun) {
      pp = P_IDLE;
    } else {
      pp = P_WAIT;
      pUntil = now + (pT.charGap > pT.elGap ? pT.charGap - pT.elGap : 0);
    }
    break;
  case P_WAIT:
    pp = P_IDLE;
    break;
  default:
    break;
  }
}

// ---------------- tune and beeps ----------------
static bool tuning = false;
static uint32_t tuneUntil = 0;
static uint8_t beepLeft = 0;
static bool beepOn = false;
static uint32_t beepAt = 0;
static bool lastPtt = false;

static void beepTick(uint32_t now) {
  if (beepReq) {
    beepLeft = (uint8_t)(beepLeft + beepReq);
    beepReq = 0;
  }
  if (!beepLeft && !beepOn)
    return;
  if (outKeyIsDown() || (int32_t)(now - beepAt) < 0)
    return;
  if (beepOn) {
    outBeep(false);
    beepOn = false;
    if (beepLeft)
      beepLeft--;
    beepAt = now + 90;
  } else if (beepLeft) {
    outBeep(true);
    beepOn = true;
    beepAt = now + 70;
  }
}

static void endTune(uint32_t now) {
  if (!tuning)
    return;
  markOff(now);
  ledsOff();
  tuning = false;
}

// ---------------- the interrupt ----------------
static repeating_timer_t timer;

static bool tick(repeating_timer_t *) {
  uint32_t now = millis();
  bool d, h, dr, hr, df, hf;
  readPaddles(d, h, dr, hr, df, hf);
  (void)df;
  (void)hf;
  bool practice = (int32_t)(practiceUntil - now) > 0;
  bool tx = !practice && config.output != OUT_OFF;

  if (config.keyer != lastMode) {
    lastMode = config.keyer;
    keyerClear(now);
  }
  if (resetReq) {
    resetReq = false;
    keyerClear(now);
  }
  if (stopReq) {
    stopReq = false;
    playerClear(now);
    endTune(now);
  }

  // Paddles mode MIDI: the raw contacts, only while transmitting is allowed.
  // A contact still "on" when that changes is released.
  static bool midiDit = false, midiDah = false;
  bool padTx = tx && config.midiMode == MIDI_PADDLES;
  bool wantDit = padTx && d, wantDah = padTx && h;
  if (wantDit != midiDit) {
    midiDit = wantDit;
    push(EV_PADDLE, wantDit ? 'd' : 'D');
  }
  if (wantDah != midiDah) {
    midiDah = wantDah;
    push(EV_PADDLE, wantDah ? 'h' : 'H');
  }

  if (tuneReq) {
    tuneReq = false;
    uint32_t ms = tuneReqMs;
    if (ms == 0) {
      endTune(now);
    } else if (!tuning && ks == K_IDLE && !mDown && pp == P_IDLE) {
      tuning = true;
      tuneUntil = now + (ms > 20000 ? 20000 : ms);
      outSetTone(config.tone);
      markOn((uint16_t)(EVK_TUNE | (tx ? EVK_TX : 0)), now);
      ledDit(true);
      ledDah(true);
    }
  }

  if (tuning) {
    if (dr || hr || (int32_t)(now - tuneUntil) >= 0)
      endTune(now);
  } else {
    beepTick(now);
    if ((d || h) && (pp != P_IDLE || tqHead != tqTail)) {
      playerClear(now);
      push(EV_ABORT, 0);
    }
    if (config.keyer == KM_STRAIGHT)
      straightTick(now, d || h, tx);
    else {
      if (dr)
        lastPress = '.';
      if (hr)
        lastPress = '-';
      autoTick(now, d, h, tx);
    }
    playerTick(now, practice);
  }

  outTick(now);
  bool p = outPttActive();
  if (p != lastPtt) {
    lastPtt = p;
    push(EV_PTT, p ? 1 : 0);
  }
  return true;
}

void engineBegin() {
  pinMode(PIN_DIT, INPUT_PULLUP);
  pinMode(PIN_DAH, INPUT_PULLUP);
  outBegin();
  calcTiming(kT, config.wpm, 0);
  pat[0] = 0;
  lastMode = config.keyer;
  keyerClear(millis());
  add_repeating_timer_us(-1000, tick, nullptr, &timer);
}

void engineStop() { stopReq = true; }
void engineResetKeyer() { resetReq = true; }
void engineBeep(uint8_t count) { beepReq = count; }

void engineTune(uint32_t ms) {
  tuneReqMs = ms;
  tuneReq = true;
}

void enginePractice(uint32_t ms) { practiceUntil = millis() + ms; }

void engineTrainSpeed(int wpm, int eff) {
  if (wpm && (wpm < 5 || wpm > 60))
    wpm = 0;
  trainEff = (eff > 0 && eff < wpm) ? eff : 0;
  trainWpm = wpm;
}
bool enginePracticeActive() { return (int32_t)(practiceUntil - millis()) > 0; }
bool enginePlayerBusy() { return pp != P_IDLE || tqHead != tqTail; }
bool engineKeyerBusy() { return ks != K_IDLE || mDown || padDit || padDah || inChar; }
bool engineTuning() { return tuning; }
bool engineBusy() { return enginePlayerBusy() || engineKeyerBusy() || tuning || beepLeft || beepOn; }
