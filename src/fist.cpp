#include "fist.h"
#include "config.h"

// Marks in ms. Gaps as a fraction of the dit, in 1/64 dit: a change of
// speed learned from the marks then moves the spacing with it at once, and
// what the gap learning tunes is the operator's own spacing habit.
static Fist f = {60, 180, 60, 180};
static uint16_t gapR = 64, cgapR = 192; // 1.0 and 3.0 dits
static Fist seeded = {0, 0, 0, 0};      // the config values the live fist was seeded from
static uint16_t lastMark = 0;           // the mark before, 0 = none (or a word break since)
static uint16_t lastGap = 0;            // the gap before (1/64 dit), 0 = none

static const uint16_t DIT_MIN = 20, DIT_MAX = 400; // 60 .. 3 wpm

// Three parts old, one part new: follows within four samples of its kind.
// A sample far off (over 30 %) that still got here moves it halfway: a new
// hand, or a cold start from the keyer speed.
static inline uint16_t ema(uint16_t old, uint16_t sample) {
  uint32_t diff = sample > old ? sample - old : old - sample;
  if (diff * 10u > (uint32_t)old * 3u)
    return (uint16_t)(((uint32_t)old + sample) / 2u);
  return (uint16_t)((3u * old + sample) / 4u);
}
static inline bool near(uint16_t v, uint16_t mean) { return v * 10u >= mean * 6u && v * 10u <= mean * 15u; }
static inline bool pairLike(uint16_t lo, uint16_t hi, unsigned minTenths, unsigned maxTimes) {
  return hi * 10u >= lo * minTenths && hi <= lo * maxTimes;
}

static void clampMarks() {
  if (f.dit < DIT_MIN)
    f.dit = DIT_MIN;
  if (f.dit > DIT_MAX)
    f.dit = DIT_MAX;
  uint32_t lo = f.dit * 16u / 10u, hi = f.dit * 6u;
  if (f.dah < lo)
    f.dah = (uint16_t)lo;
  if (f.dah > hi)
    f.dah = (uint16_t)hi;
}

// gap 0.4 .. 2.5 dits; cgap at least a dit above it and 1.5 dits, at most 8.
static void clampGaps() {
  if (gapR < 26)
    gapR = 26;
  if (gapR > 160)
    gapR = 160;
  uint16_t clo = (uint16_t)(gapR + 64);
  if (clo < 96)
    clo = 96;
  if (cgapR < clo)
    cgapR = clo;
  if (cgapR > 512)
    cgapR = 512;
  f.gap = (uint16_t)(((uint32_t)f.dit * gapR) / 64u);
  f.cgap = (uint16_t)(((uint32_t)f.dit * cgapR) / 64u);
}

static inline uint16_t toR(uint16_t ms) { return (uint16_t)(((uint32_t)ms * 64u) / f.dit); }

static void seedFrom(int wpm) {
  if (wpm < 3)
    wpm = 18;
  f.dit = (uint16_t)(1200 / wpm);
  f.dah = (uint16_t)(f.dit * 3u);
  gapR = 64;
  cgapR = 192;
  clampMarks();
  clampGaps();
}

void fistSeed(int wpm) {
  seeded.dit = (uint16_t)config.fistDit;
  seeded.dah = (uint16_t)config.fistDah;
  seeded.gap = (uint16_t)config.fistGap;
  seeded.cgap = (uint16_t)config.fistCharGap;
  if (seeded.dit) {
    f.dit = seeded.dit;
    f.dah = seeded.dah ? seeded.dah : (uint16_t)(f.dit * 3u);
    clampMarks();
    gapR = seeded.gap ? toR(seeded.gap) : 64;
    cgapR = seeded.cgap ? toR(seeded.cgap) : 192;
    clampGaps();
  } else {
    seedFrom(wpm);
  }
  lastMark = lastGap = 0;
}

void fistTick() {
  if ((uint16_t)config.fistDit != seeded.dit || (uint16_t)config.fistDah != seeded.dah ||
      (uint16_t)config.fistGap != seeded.gap || (uint16_t)config.fistCharGap != seeded.cgap)
    fistSeed(config.wpm);
}

char fistMark(uint16_t ms) {
  if (ms == 0)
    ms = 1;
  char e = fistClassify(ms);
  uint16_t lo = ms, hi = lastMark;
  if (hi < lo) {
    lo = lastMark;
    hi = ms;
  }
  if (lastMark && pairLike(lo, hi, 17, 5)) { // one of each, whatever the speed
    f.dit = ema(f.dit, lo);
    f.dah = ema(f.dah, hi);
  } else if (e == '.' && near(ms, f.dit)) {
    f.dit = ema(f.dit, ms);
  } else if (e == '-' && near(ms, f.dah)) {
    f.dah = ema(f.dah, ms);
  }
  clampMarks();
  clampGaps();
  lastMark = ms;
  return e;
}

void fistKeyerMark(uint16_t ms) { fistMark(ms); }

char fistClassify(uint16_t ms) { return ms * 2u < (uint32_t)f.dit + f.dah ? '.' : '-'; }

void fistSpace(uint16_t ms) {
  if (ms == 0 || ms >= fistWordEnd()) {
    lastGap = 0;
    return;
  }
  bool inLetter = ms < fistCharEnd();
  uint16_t r = toR(ms);
  if (r == 0)
    r = 1;
  uint16_t lo = r, hi = lastGap;
  if (hi < lo) {
    lo = lastGap;
    hi = r;
  }
  if (lastGap && pairLike(lo, hi, 15, 6)) { // 1.5: a tight fist has 1.4 and 2.4
    gapR = ema(gapR, lo);
    cgapR = ema(cgapR, hi);
  } else if (inLetter && near(r, gapR)) {
    gapR = ema(gapR, r);
  } else if (!inLetter && near(r, cgapR)) {
    cgapR = ema(cgapR, r);
  }
  clampGaps();
  lastGap = r;
}

void fistWordBreak() { lastMark = lastGap = 0; }

// Sixty percent of the way from the gap inside a letter to the gap between
// letters: 2.2 dits for a 1:3 fist, nearer 3 for one that swings.
uint16_t fistCharEnd() { return (uint16_t)(f.gap + ((uint32_t)(f.cgap - f.gap) * 3u) / 5u); }

// Halfway between the letter gap and a word gap of twice that plus a dit:
// 5 dits for a 1:3 fist, whose words are 7 apart. Low enough that a word gap
// is never taken for a letter gap and learned as one, which would run away.
uint16_t fistWordEnd() { return (uint16_t)(((uint32_t)f.cgap * 3u) / 2u + f.dit / 2u); }

uint8_t fistFit(const char *pat, const uint16_t *ms, const uint16_t *gap, uint16_t hand, uint8_t n) {
  uint32_t worst = 0;
  uint32_t half = (f.dah - f.dit) / 2u;        // cluster to the dit/dah line
  uint32_t gspan = fistCharEnd() - f.gap;       // gap inside a letter to the letter-end line
  uint32_t cspan = f.cgap - fistCharEnd();      // letter gap down to that line
  if (half < 1)
    half = 1;
  if (gspan < 1)
    gspan = 1;
  if (cspan < 1)
    cspan = 1;
  for (uint8_t i = 0; i < n; i++) {
    uint32_t p = 0;
    if (hand & (1u << i)) {
      uint16_t target = pat[i] == '.' ? f.dit : f.dah;
      uint32_t d = ms[i] > target ? ms[i] - target : target - ms[i];
      p = d * 100u / half;
      if (p > worst)
        worst = p;
    }
    p = 0;
    if (gap[i] == 0)
      continue;
    if (i == 0) {
      if (gap[i] < fistWordEnd() && gap[i] < f.cgap)
        p = (f.cgap - gap[i]) * 100u / cspan;
    } else if (gap[i] > f.gap) {
      p = (gap[i] - f.gap) * 100u / gspan;
    }
    if (p > worst)
      worst = p;
  }
  return worst > 250 ? 250 : (uint8_t)worst;
}

const Fist &fistNow() { return f; }

bool fistSaved() { return config.fistDit > 0; }

void fistSave() {
  config.fistDit = f.dit;
  config.fistDah = f.dah;
  config.fistGap = f.gap;
  config.fistCharGap = f.cgap;
  seeded = f;
  configMarkDirty();
}

void fistForget() {
  config.fistDit = config.fistDah = config.fistGap = config.fistCharGap = 0;
  configMarkDirty();
  fistSeed(config.wpm);
}
