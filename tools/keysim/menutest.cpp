// Host test of the menu on the box (src/menu.cpp) with a stubbed config table and engine.
// Build: g++ -std=c++17 -I. -I../../src menutest.cpp -o menutest
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stddef.h>
#include "Arduino.h"
uint32_t simNow = 1000;
#define fakeMs simNow
struct RP2040Stub { int reboots = 0; void reboot() { reboots++; } } rp2040;
#include "config.h"
Config config;
#define OFF(f) ((uint16_t)offsetof(Config, f))
static const CfgItem cfgTab[] = {
    {"volume", CT_INT, OFF(volume), 1, 10, 10, nullptr, nullptr, "sound", 0, ""},
    {"lineLevel", CT_INT, OFF(lineLevel), 1, 10, 10, nullptr, nullptr, "sound", 0, ""},
    {"tone", CT_INT, OFF(tone), 300, 2000, 600, nullptr, nullptr, "sound", 0, ""},
    {"keyer", CT_ENUM, OFF(keyer), 0, 0, 1, nullptr, "iambicA|iambicB|ultimatic|bug|straight", "keyer", 0, ""},
    {"paddleSwap", CT_BOOL, OFF(paddleSwap), 0, 1, 0, nullptr, nullptr, "keyer", 0, ""},
    {"output", CT_ENUM, OFF(output), 0, 0, 0, nullptr, "key|audio|off", "output", 0, ""},
    {"buzzer", CT_BOOL, OFF(buzzer), 0, 1, 1, nullptr, nullptr, "sound", 0, ""},
    {"keyboard", CT_BOOL, OFF(keyboard), 0, 1, 0, nullptr, nullptr, "computer", CF_REBOOT, ""},
};
const CfgItem *cfgFind(const char *key) { for (auto &i : cfgTab) if (!strcmp(i.key, key)) return &i; return nullptr; }
static std::string enumName(const CfgItem *it, int idx) {
  const char *s = it->opts; while (idx-- > 0) { while (*s && *s != '|') s++; if (*s) s++; }
  const char *e = s; while (*e && *e != '|') e++; return std::string(s, e);
}
static char enumBuf[24];
const char *cfgEnumName(const CfgItem *it, int idx) { strncpy(enumBuf, enumName(it, idx).c_str(), 23); return enumBuf; }
const char *cfgSet(const char *key, const char *value) {
  const CfgItem *it = cfgFind(key); assert(it); uint8_t *p = (uint8_t *)&config + it->offset;
  if (it->type == CT_INT) *(int *)p = atoi(value);
  else if (it->type == CT_BOOL) *(bool *)p = !strcmp(value, "on");
  else { int n = 0; for (;; n++) { if (enumName(it, n) == value) { *(int *)p = n; break; } assert(n < 8); } }
  return nullptr;
}
String cfgGet(const CfgItem *it) { const uint8_t *p = (const uint8_t *)&config + it->offset;
  if (it->type == CT_INT) return std::to_string(*(const int *)p); if (it->type == CT_BOOL) return *(const bool *)p ? "on" : "off"; return enumName(it, *(const int *)p); }
static int dirty = 0, saved = 0; void configMarkDirty() { dirty++; } bool configSave() { saved++; return true; }
static std::vector<std::string> said; static bool busy = false;
void engineStop() {} size_t engineQueue(const char *t, uint8_t) { said.push_back(t); return 1; } bool engineBusy() { return busy; }
static std::string stream; void hubStream(const char *s) { stream += s; } void hubStream(char c) { stream += c; } void hubStateChanged() {}
void outApplyConfig() {} void wkUserSet(const CfgItem *) {}
#include "../../src/engine.h"
#include "../../src/menu.h"
#include "../../src/menu.cpp"

static std::string last() { return said.empty() ? "" : said.back(); }
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
  memset(&config, 0, sizeof config);
  config.volume = 10; config.lineLevel = 10; config.tone = 600; config.keyer = KM_IAMBIC_B; config.buzzer = true;
  // enter, walk the items both ways with wrap
  menuEnter(); CHECK(menuActive()); CHECK(last() == "VOL");
  menuPaddle('-'); CHECK(last() == "LINE");
  for (int i = 0; i < 7; i++) menuPaddle('-'); CHECK(last() == "VOL");
  menuPaddle('.'); CHECK(last() == "KBD");
  menuPaddle('b'); CHECK(last() == "KBD"); // a squeeze inside does nothing
  // LINE: open, step down to the floor, save
  menuPaddle('-'); menuPaddle('-'); CHECK(last() == "LINE");
  menuButton(); CHECK(last() == "10");
  menuPaddle('.'); CHECK(last() == "9"); CHECK(config.lineLevel == 9);
  for (int i = 0; i < 12; i++) menuPaddle('.'); CHECK(last() == "1"); CHECK(config.lineLevel == 1);
  menuPaddle('-'); CHECK(last() == "2");
  int d0 = dirty; menuButton(); CHECK(last() == "R"); CHECK(!menuActive()); CHECK(dirty == d0 + 1); CHECK(config.lineLevel == 2);
  CHECK(stream == "!menu lineLevel 2\n");
  menuPaddle('-'); CHECK(last() == "R"); // closed: paddles do nothing here
  // SWAP: flip, then cancel puts it back
  menuEnter(); for (int i = 0; i < 4; i++) menuPaddle('-'); CHECK(last() == "SWAP");
  menuButton(); CHECK(last() == "OFF");
  menuPaddle('.'); CHECK(last() == "ON"); CHECK(config.paddleSwap);
  menuCancel(); CHECK(last() == "<SK>"); CHECK(!config.paddleSwap); CHECK(!menuActive());
  // KEY: a choice wraps
  menuEnter(); menuPaddle('-'); menuPaddle('-'); menuPaddle('-'); CHECK(last() == "KEY");
  menuButton(); CHECK(last() == "B");
  menuPaddle('-'); CHECK(last() == "U"); CHECK(config.keyer == KM_ULTIMATIC);
  menuPaddle('-'); menuPaddle('-'); menuPaddle('-'); CHECK(last() == "A"); CHECK(config.keyer == KM_IAMBIC_A);
  menuPaddle('.'); CHECK(last() == "STR");
  menuButton(); CHECK(config.keyer == KM_STRAIGHT);
  // TONE steps by 50 and clamps at the top
  menuEnter(); menuPaddle('-'); menuPaddle('-'); CHECK(last() == "TONE");
  menuButton(); CHECK(last() == "600"); menuPaddle('-'); CHECK(last() == "650");
  for (int i = 0; i < 40; i++) menuPaddle('-'); CHECK(last() == "2000"); CHECK(config.tone == 2000);
  menuCancel(); CHECK(config.tone == 600);
  // timeout cancels
  menuEnter(); menuPaddle('-'); menuButton(); menuPaddle('.'); CHECK(config.lineLevel == 1);
  fakeMs += 31000; menuTick(); CHECK(last() == "<SK>"); CHECK(!menuActive()); CHECK(config.lineLevel == 2);
  // KBD saves, then restarts once the R has been heard
  menuEnter(); menuPaddle('.'); CHECK(last() == "KBD");
  menuButton(); CHECK(last() == "OFF"); menuPaddle('-'); CHECK(last() == "ON"); CHECK(config.keyboard);
  menuButton(); CHECK(last() == "R");
  busy = true; menuTick(); CHECK(rp2040.reboots == 0);
  busy = false; menuTick(); fakeMs += 200; menuTick(); CHECK(rp2040.reboots == 0);
  fakeMs += 300; menuTick(); CHECK(rp2040.reboots == 1); CHECK(saved == 1);
  printf("%s: %d checks failed\n", fails ? "MENU TEST FAILED" : "menu test ok", fails);
  return fails ? 1 : 0;
}
