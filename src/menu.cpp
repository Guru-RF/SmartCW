#include "menu.h"
#include "config.h"
#include "engine.h"
#include "hub.h"
#include "output.h"
#include "wk.h"

struct Item {
  const char *name; // said in CW
  const char *key;  // config.txt key
  int step;         // for a number
  const char *said; // '|' separated names for a switch or a choice, nullptr = say the number
};
static const Item items[] = {
    {"VOL", "volume", 1, nullptr},
    {"LINE", "lineLevel", 1, nullptr},
    {"TONE", "tone", 50, nullptr},
    {"KEY", "keyer", 1, "A|B|U|BUG|STR"},
    {"SWAP", "paddleSwap", 1, "OFF|ON"},
    {"OUT", "output", 1, "KEY|AUD|OFF"},
    {"BUZ", "buzzer", 1, "OFF|ON"},
    {"KBD", "keyboard", 1, "OFF|ON"}, // last: saving it restarts SmartCW
};
static const int N = sizeof(items) / sizeof(items[0]);
static const uint32_t TIMEOUT_MS = 30000;

enum State { S_OFF, S_ITEMS, S_VALUE };
static State st = S_OFF;
static int sel = 0;
static int orig = 0;          // the value on entering an item, for cancel
static uint32_t lastAct = 0;
static bool rebootPending = false;
static uint32_t rebootAt = 0;

static const CfgItem *item() { return cfgFind(items[sel].key); }

static int get(const CfgItem *it) {
  const uint8_t *p = (const uint8_t *)&config + it->offset;
  return it->type == CT_BOOL ? (*(const bool *)p ? 1 : 0) : *(const int *)p;
}

static int count(const char *opts) {
  int n = 1;
  for (const char *s = opts; *s; s++)
    if (*s == '|')
      n++;
  return n;
}

// The i-th '|' separated word of s.
static void nth(const char *s, int i, char *out, size_t len) {
  while (i-- > 0 && *s) {
    while (*s && *s != '|')
      s++;
    if (*s)
      s++;
  }
  size_t k = 0;
  while (*s && *s != '|' && k + 1 < len)
    out[k++] = *s++;
  out[k] = 0;
}

static void say(const char *text) {
  engineStop(); // a fresh tap cuts the previous answer short
  engineQueue(text, PROF_PRACTICE);
}

static void sayValue() {
  const CfgItem *it = item();
  char b[12];
  if (items[sel].said)
    nth(items[sel].said, get(it), b, sizeof(b));
  else
    snprintf(b, sizeof(b), "%d", get(it));
  say(b);
}

static void set(const CfgItem *it, int v) {
  char b[12];
  const char *s = b;
  if (it->type == CT_ENUM)
    s = cfgEnumName(it, v);
  else if (it->type == CT_BOOL)
    s = v ? "on" : "off";
  else
    snprintf(b, sizeof(b), "%d", v);
  cfgSet(it->key, s); // applies live: the next answer is heard at the new value
  outApplyConfig();
  hubStateChanged();
}

void menuEnter() {
  if (st != S_OFF)
    return;
  st = S_ITEMS;
  sel = 0;
  lastAct = millis();
  say(items[sel].name);
}

void menuPaddle(char c) {
  if (st == S_OFF || c == 'b')
    return;
  lastAct = millis();
  int dir = c == '-' ? 1 : -1;
  if (st == S_ITEMS) {
    sel = (sel + dir + N) % N;
    say(items[sel].name);
    return;
  }
  const CfgItem *it = item();
  int v = get(it);
  if (it->type == CT_BOOL) {
    v = !v;
  } else if (it->type == CT_ENUM) {
    int n = count(it->opts);
    v = (v + dir + n) % n;
  } else {
    v += dir * items[sel].step;
    if (v < it->min)
      v = (int)it->min;
    if (v > it->max)
      v = (int)it->max;
  }
  if (v != get(it))
    set(it, v);
  sayValue();
}

void menuButton() {
  if (st == S_OFF)
    return;
  lastAct = millis();
  const CfgItem *it = item();
  if (st == S_ITEMS) {
    st = S_VALUE;
    orig = get(it);
    sayValue();
    return;
  }
  wkUserSet(it); // the operator's choice: kept when a logger closes
  configMarkDirty();
  hubStream("!menu ");
  hubStream(it->key);
  hubStream(" ");
  hubStream(cfgGet(it).c_str());
  hubStream("\n");
  st = S_OFF;
  say("R");
  if (it->flags & CF_REBOOT)
    rebootPending = true; // after the R has been heard
}

void menuCancel() {
  if (st == S_OFF)
    return;
  if (st == S_VALUE && get(item()) != orig)
    set(item(), orig);
  st = S_OFF;
  say("<SK>");
}

void menuTick() {
  uint32_t now = millis();
  if (st != S_OFF && now - lastAct > TIMEOUT_MS)
    menuCancel();
  if (rebootPending) {
    if (engineBusy()) {
      rebootAt = 0;
    } else if (!rebootAt) {
      rebootAt = now + 400;
    } else if ((int32_t)(now - rebootAt) >= 0) {
      configSave();
      rp2040.reboot();
    }
  }
}

bool menuActive() { return st != S_OFF; }
