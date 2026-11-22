// Host-side test of the SmartCW decoder with hand-keyed fists.
// Build: g++ -std=c++17 -O1 -I. -I../../src sim.cpp -o sim
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <random>
#include <string>
#include <vector>
uint32_t simNow = 0;
bool simDit = false, simDah = false;

#include "config.h"
Config config;
static bool dirty = false;
void configMarkDirty() { dirty = true; }
int configDitMs() { return 1200 / config.wpm; }
#include "output.h"
void outBegin() {}
void outSetTone(int) {}
void outKey(bool, bool, uint32_t) {}
void outBeep(bool) {}
void outTick(uint32_t) {}
void outAllOff() {}
bool outPttActive() { return false; }
bool outKeyIsDown() { return false; }
#include "leds.h"
void ledsBegin() {}
void ledPwr(bool) {}
void ledDit(bool) {}
void ledDah(bool) {}
void ledSer(bool) {}
void ledSer2(bool) {}

#include "fist.cpp"
#include "morse.cpp"
#include "engine.cpp"

static std::string rx;
static int evCount = 0, nq = 0, unsure = 0, qsum = 0;
static void drain() {
  Event e;
  while (enginePoll(e)) {
    evCount++;
    if (e.type == EV_RX) {
      rx += e.a ? (char)toupper((char)e.a) : '?';
      if (e.b) { nq++; qsum += e.c; if (e.c >= 70) unsure++; }
    }
  }
}
static void run(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    simNow++;
    tick(nullptr);
    drain();
  }
}
static void key(bool down, uint32_t ms) { simDit = down; run(ms); }

// A fist: dit ms, dah in dits, gap inside a letter in dits, gap between letters, between words, jitter (fraction).
struct FistP { const char *name; double dit, dah, gap, cgap, wgap, jit; };
static std::mt19937 rng(7);
static double jit(double v, double j) {
  if (j <= 0) return v;
  std::normal_distribution<double> n(0, j);
  double r = v * (1 + n(rng));
  return r < 5 ? 5 : r;
}
static void sendText(const std::string &text, const FistP &p) {
  bool first = true;
  for (size_t i = 0; i < text.size(); i++) {
    char c = text[i];
    if (c == ' ') { key(false, (uint32_t)jit(p.wgap * p.dit, p.jit) - (uint32_t)jit(p.cgap * p.dit, 0)); continue; }
    const char *pat = morseEncode(c);
    if (!pat) continue;
    if (!first) key(false, (uint32_t)jit(p.cgap * p.dit, p.jit));
    first = false;
    for (const char *e = pat; *e; e++) {
      if (e != pat) key(false, (uint32_t)jit(p.gap * p.dit, p.jit));
      key(true, (uint32_t)jit((*e == '.' ? 1.0 : p.dah) * p.dit, p.jit));
    }
  }
  key(false, 2000);
}
static int errors(const std::string &a, const std::string &b) {
  // Levenshtein
  std::vector<std::vector<int>> d(a.size() + 1, std::vector<int>(b.size() + 1));
  for (size_t i = 0; i <= a.size(); i++) d[i][0] = i;
  for (size_t j = 0; j <= b.size(); j++) d[0][j] = j;
  for (size_t i = 1; i <= a.size(); i++)
    for (size_t j = 1; j <= b.size(); j++)
      d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
  return d[a.size()][b.size()];
}
static std::string trim(std::string s) {
  while (!s.empty() && s.back() == ' ') s.pop_back();
  while (!s.empty() && s.front() == ' ') s.erase(s.begin());
  return s;
}
static int total = 0, failed = 0;
static void test(const char *what, const std::string &text, const FistP &p, bool resetFist = true) {
  if (resetFist) { config.fistDit = config.fistDah = config.fistGap = config.fistCharGap = 0; fistSeed(config.wpm); }
  rx.clear();
  nq = unsure = qsum = 0;
  sendText(text, p);
  std::string got = trim(rx);
  int e = errors(text, got);
  total++;
  if (e > 0) failed++;
  const Fist &f = fistNow();
  printf("%-34s %-14s errors %2d  fit avg %3d unsure %2d/%-2d  learned dit %3u dah %3u gap %3u cgap %3u  got: %s\n", what, p.name, e,
         nq ? qsum / nq : 0, unsure, nq, f.dit, f.dah, f.gap, f.cgap, got.c_str());
}

int main() {
  memset(&config, 0, sizeof(config));
  config.weight = 50; config.ratio = 30; config.tone = 600;
  config.keyer = KM_STRAIGHT;
  config.wpm = 18;
  engineBegin();
  const std::string T = "CQ CQ DE ON6URE ON6URE K";
  const std::string T2 = "THE QUICK BROWN FOX 73 OM";
  FistP std20{"std 20wpm", 60, 3, 1, 3, 7, 0.0};
  FistP shortDah{"dah 2.2", 60, 2.2, 1, 3, 7, 0.05};
  FistP longDah{"dah 4.2", 60, 4.2, 1.2, 3.5, 8, 0.05};
  FistP tight{"tight spacing", 60, 3, 1, 2.0, 5, 0.05};
  FistP swing{"swing", 70, 3.2, 1.8, 4.2, 9, 0.08};
  FistP slow{"slow 10wpm", 120, 3, 1.2, 3.2, 7, 0.08};
  FistP fast{"fast 28wpm", 43, 2.8, 0.9, 2.8, 6, 0.06};
  FistP sloppy{"sloppy", 65, 2.6, 1.3, 2.6, 6, 0.15};
  FistP ot{"OT: long dah, tight", 75, 3.8, 1.4, 2.4, 6, 0.1};
  test("plain", T, std20);
  test("plain 2", T2, std20);
  test("short dahs", T, shortDah);
  test("long dahs", T, longDah);
  test("tight letters", T, tight);
  test("swinging", T, swing);
  test("slow", T, slow);
  test("fast", T, fast);
  test("sloppy", T, sloppy);
  test("sloppy 2", T2, sloppy);
  test("OT", T, ot);
  test("OT 2", T2, ot);
  // hand-over: slow operator then a fast one without reseeding
  test("hand-over: slow first", T, slow);
  test("hand-over: then fast", T, fast, false);
  test("hand-over: then OT", T2, ot, false);
  test("hand-over: then slow", T2, slow, false);
  // runs of dahs and dits
  test("dah runs", "OOO 000 MMM TTT", std20);
  test("dit runs", "HHH 555 SSS EEE", std20);
  test("dah runs OT", "OOO 000 MMM TTT", ot);
  test("dit runs sloppy", "HHH 555 SSS EEE", sloppy);
  // bug mode: automatic dits at the keyer speed, hand-made dahs
  config.keyer = KM_BUG;
  config.wpm = 20;
  engineResetKeyer();
  run(10);
  printf("(bug mode tests use the dah paddle for dahs and the dit paddle, held, for dits)\n");
  // in bug mode the sim presses dah for a dah and dit for the keyer's dits: emulate with a custom sender
  {
    auto bugSend = [&](const std::string &text, double dahMs, double gapD, double cgapD) {
      bool first = true;
      for (char c : text) {
        if (c == ' ') { run(7 * 60); continue; }
        const char *pat = morseEncode(c);
        if (!pat) continue;
        if (!first) run((uint32_t)(cgapD * 60));
        first = false;
        for (const char *e = pat; *e; e++) {
          if (e != pat) run((uint32_t)(gapD * 60));
          if (*e == '.') { simDit = true; run(70); simDit = false; } // held for one dit: the keyer makes one
          else { simDah = true; run((uint32_t)dahMs); simDah = false; }
        }
      }
      run(2000);
    };
    config.fistDit = 0; fistSeed(config.wpm);
    rx.clear(); bugSend(T, 180, 1.0, 3.0);
    printf("%-34s errors %2d got: %s\n", "bug: 3:1", errors(T, trim(rx)), trim(rx).c_str());
    total++; if (errors(T, trim(rx))) failed++;
    rx.clear(); bugSend(T, 260, 1.3, 2.6);
    printf("%-34s errors %2d got: %s\n", "bug: long dahs, tight", errors(T, trim(rx)), trim(rx).c_str());
    total++; if (errors(T, trim(rx))) failed++;
  }
  // saved fist: seeded from config
  config.keyer = KM_STRAIGHT;
  engineResetKeyer();
  run(10);
  config.fistDit = 75; config.fistDah = 285; config.fistGap = 105; config.fistCharGap = 180;
  run(5); // fistTick picks it up
  const Fist &f = fistNow();
  printf("seeded from config: dit %u dah %u gap %u cgap %u\n", f.dit, f.dah, f.gap, f.cgap);
  test("OT from saved fist", T, ot, false);

  // iambic B: the keyer makes the marks, the operator makes the spacing
  config.keyer = KM_IAMBIC_B;
  config.wpm = 20;
  engineResetKeyer();
  run(10);
  {
    auto padSend = [&](const std::string &text, double extraEl, double extraCh, double wordD, double j) {
      bool first = true;
      for (char c : text) {
        if (c == ' ') { run((uint32_t)jit(wordD * 60, j)); continue; }
        const char *pat = morseEncode(c);
        if (!pat) continue;
        if (!first) run((uint32_t)jit(extraCh * 60, j));
        first = false;
        for (const char *e = pat; *e; e++) {
          if (e != pat) run((uint32_t)jit(extraEl * 60, j));
          if (*e == '.') { simDit = true; run(15); simDit = false; run(60 + 60); } // the element and the keyer's gap
          else { simDah = true; run(15); simDah = false; run(180 + 60); }
        }
      }
      run(2000);
    };
    struct PT { const char *name; double el, ch, word, j; } pts[] = {
      {"paddles: standard spacing", 0.0, 2.0, 7, 0.0},
      {"paddles: hesitant", 1.0, 3.0, 8, 0.1},
      {"paddles: tight letters", 0.2, 1.0, 5, 0.1},
      {"paddles: very hesitant", 1.5, 4.5, 10, 0.1},
    };
    for (auto &pt : pts) {
      config.fistDit = 0; fistSeed(config.wpm);
      rx.clear(); padSend(T, pt.el, pt.ch, pt.word, pt.j);
      int e = errors(T, trim(rx));
      const Fist &f = fistNow();
      printf("%-34s errors %2d  learned dit %3u dah %3u gap %3u cgap %3u  got: %s\n", pt.name, e, f.dit, f.dah, f.gap, f.cgap, trim(rx).c_str());
      total++; if (e) failed++;
    }
  }
  printf("\n%d tests, %d with errors\n", total, failed);
  return failed ? 1 : 0;
}
