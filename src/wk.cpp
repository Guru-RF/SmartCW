// WinKeyer 2 (v23) host protocol, after the K1EL WK2/WK3 datasheets.
#include "wk.h"
#include "cli.h"
#include "config.h"
#include "feeder.h"
#include "hub.h"
#include "output.h"
#include <tusb.h>

static const uint8_t WK_VERSION = 23;

// Loggers open the port at 1200 baud (WinKeyer's rate) and drop DTR when
// they close it: that is the Arduino "1200 baud touch", which reboots the
// RP2040 into the UF2 bootloader. With WinKeyer answers on, 1200 baud is
// reported to the core as 1201 (USB ignores the rate anyway).
extern "C" void __real_tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p);
extern "C" void __wrap_tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p) {
  if (p->bit_rate == 1200 && config.winkeyer) {
    cdc_line_coding_t c = *p;
    c.bit_rate = 1201;
    __real_tud_cdc_line_coding_cb(itf, &c);
    return;
  }
  __real_tud_cdc_line_coding_cb(itf, p);
}

enum WkState : uint8_t { WK_OFF, WK_CLOSED, WK_OPEN };
static WkState st = WK_OFF;

// parser
static uint16_t cmd = 0; // command waiting for parameters, 0x100 | n for admin n
static uint8_t need = 0, npar = 0;
static uint8_t par[16];
static bool adminNext = false, echoNext = false;
static uint16_t eepromLeft = 0;
static uint32_t lastAdmin = 0;
static bool dtrSeen = false;

// registers, as the logger set them (Get Values returns them)
static uint8_t regMode, regSpeed, regTone, regWeight, regLead, regTail, regMin = 5, regRange = 30, regExt,
    regComp, regFarns, regSamp = 50, regRatio = 50, regPin, regX1, regX2;
static bool wk2Mode = false, wk3Mode = false, serialEcho = false, paddleEcho = false;

static TxFeeder feed;
static Config saved; // the operator's own settings during a session
static char pendingEcho = 0;
static uint8_t lastStatus = 0;
static uint32_t lastPaddle = 0;
static bool paddleSeen = false, wasBreakin = false;

// The settings a logger may change; restored when it closes.
#define WK_FIELDS(X) \
  X(wpm) X(weight) X(ratio) X(keyer) X(paddleSwap) X(autoSpace) X(tone) X(buzzer) X(pttLead) X(pttTail) X(keyComp) X(firstExt)
#define WK_COPY(f) dst.f = src.f;
static void copyFields(Config &dst, const Config &src) { WK_FIELDS(WK_COPY) }

static void out(uint8_t b) {
  if (Serial.availableForWrite() > 0)
    Serial.write(b);
}

bool wkActive() { return st != WK_OFF; }
bool wkOpen() { return st == WK_OPEN; }

void wkPersistView(Config &c) {
  if (st == WK_OPEN)
    copyFields(c, saved);
  if (feed.baseWpm())
    c.wpm = feed.baseWpm();
}

// A setting changed on the web page or a command during a session: it is
// the operator's, so it stays after the logger closes.
void wkUserSet(const CfgItem *it) {
  if (st != WK_OPEN || it->type == CT_STR)
    return;
  size_t n = it->type == CT_BOOL ? sizeof(bool) : sizeof(int);
  memcpy((uint8_t *)&saved + it->offset, (const uint8_t *)&config + it->offset, n);
}

static void applied() { cliApplied(""); }

// ---------------- characters ----------------
// Host byte -> SmartCW character (0 = ignored). ';' (AA) is sent as a run.
static char mapIn(uint8_t c) {
  if (c >= 'a' && c <= 'z')
    c = (uint8_t)(c - 32);
  if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
    return (char)c;
  switch (c) {
  case ' ': case '.': case ',': case '?': case '/': case '"': case '$':
  case '\'': case '(': case ')': case '+': case '-': case '=': case '@':
    return (char)c;
  case ':': case ']':
    return '('; // KN
  case '<':
    return '+'; // AR
  case '>':
    return '|'; // SK
  case '[':
    return '&'; // AS
  case '\\':
    return '/'; // DN
  }
  return 0;
}

// SmartCW character -> echo byte for the host.
static char mapOut(char c) {
  if (c >= 'a' && c <= 'z')
    return (char)(c - 32);
  switch (c) {
  case '|':
    return '>';
  case '&':
    return '[';
  case '*': case '^': case '#':
    return 0;
  }
  return c;
}

// ---------------- settings ----------------
static uint8_t potByte() {
  int v = config.wpm - regMin;
  int top = regRange < 31 ? regRange : 31;
  return (uint8_t)(0x80 | constrain(v, 0, top));
}

static void setMode(uint8_t m) {
  regMode = m;
  paddleEcho = m & 0x40;
  serialEcho = m & 0x04;
  config.paddleSwap = m & 0x08;
  config.autoSpace = m & 0x02;
  engineContestSpacing(m & 0x01);
  // A straight key stays a straight key; WinKeyer has no such mode.
  if (config.keyer != KM_STRAIGHT) {
    static const int km[4] = {KM_IAMBIC_B, KM_IAMBIC_A, KM_ULTIMATIC, KM_BUG};
    config.keyer = km[(m >> 4) & 3];
  }
}

static void setSpeed(uint8_t v) {
  regSpeed = v;
  feed.forgetSpeed();
  if (v) // 0 = follow the speed pot: SmartCW's own speed
    config.wpm = constrain((int)v, 5, 99);
}

static void setTone(uint8_t v) {
  regTone = v;
  int hz = 0;
  if (wk3Mode)
    hz = v ? 62500 / v : 0;
  else if (v & 0x0f)
    hz = 4000 / (v & 0x0f);
  if (hz)
    config.tone = constrain(hz, 300, 2000);
}

static void setLeadTail(uint8_t lead, uint8_t tail) {
  regLead = lead;
  regTail = tail;
  config.pttLead = constrain(lead * 10, 0, 500);
  config.pttTail = constrain(tail * 10 + 3 * 1200 / config.wpm, 0, 3000);
}

static void setPin(uint8_t v) {
  regPin = v;
  config.buzzer = (v & 0x02) ? saved.buzzer : false;
}

static void setWeight(uint8_t v) {
  regWeight = v;
  config.weight = constrain((int)v, 25, 75);
}

static void setRatio(uint8_t v) {
  regRatio = v;
  config.ratio = constrain((v * 3 + 2) / 5, 25, 45); // 50 = 1:3
}

static void setExt(uint8_t v) {
  regExt = v;
  config.firstExt = v > 150 ? 150 : v;
}

static void setComp(uint8_t v) {
  regComp = v;
  config.keyComp = v > 50 ? 50 : v;
}

static void loadDefaults(const uint8_t *v) {
  setMode(v[0]);
  setSpeed(v[1]);
  setTone(v[2]);
  setWeight(v[3]);
  setLeadTail(v[4], v[5]);
  regMin = v[6];
  regRange = v[7];
  if (wk3Mode)
    regX2 = v[8];
  else
    setExt(v[8]);
  setComp(v[9]);
  regFarns = v[10];
  regSamp = v[11];
  setRatio(v[12]);
  setPin(v[13]);
  regX1 = v[14];
}

// ---------------- session ----------------
static void parserReset() {
  cmd = 0;
  need = npar = 0;
  adminNext = echoNext = false;
  eepromLeft = 0;
}

static void hostOpen() {
  if (st != WK_OPEN) {
    saved = config;
    st = WK_OPEN;
  }
  wk2Mode = wk3Mode = false;
  regMode = (uint8_t)((config.keyer == KM_IAMBIC_A ? 0x10 : config.keyer == KM_ULTIMATIC ? 0x20 : config.keyer == KM_BUG ? 0x30 : 0) |
                      (config.paddleSwap ? 0x08 : 0) | (config.autoSpace ? 0x02 : 0));
  regSpeed = (uint8_t)config.wpm;
  regTone = (uint8_t)constrain(4000 / config.tone, 1, 10);
  regWeight = (uint8_t)config.weight;
  regLead = (uint8_t)(config.pttLead / 10);
  regTail = 0;
  regExt = (uint8_t)config.firstExt;
  regComp = (uint8_t)config.keyComp;
  regRatio = (uint8_t)(config.ratio * 5 / 3);
  regPin = config.buzzer ? 0x0a : 0x08;
  serialEcho = paddleEcho = false;
  lastStatus = 0;
  pendingEcho = 0;
  out(WK_VERSION);
  hubStateChanged();
}

static void endSession() {
  if (st == WK_OPEN) {
    feed.clear();
    engineStop();
    engineSoftPaddle(0);
    engineContestSpacing(false);
    copyFields(config, saved);
    applied();
  }
  st = WK_OFF;
  parserReset();
  Serial.ignoreFlowControl(false);
  hubStateChanged();
}

static void sendStatus(bool force) {
  if (st != WK_OPEN)
    return;
  uint32_t now = millis();
  bool breakin = engineKeyerBusy() || (paddleSeen && now - lastPaddle < 7u * (uint32_t)configDitMs());
  uint8_t s = 0xc0;
  if (feed.waiting())
    s |= 0x10;
  if (!wk2Mode && engineTuning())
    s |= 0x08;
  if (feed.count() || enginePlayerBusy() || feed.waiting() || engineTuning())
    s |= 0x04;
  if (breakin)
    s |= 0x02;
  if (feed.count() > TxFeeder::CAP * 2 / 3)
    s |= 0x01;
  if (force || s != lastStatus) {
    out(s);
    lastStatus = s;
  }
}

// ---------------- commands ----------------
static uint8_t paramCount(uint8_t c) {
  static const uint8_t n[32] = {0, 1, 1, 1, 2, 3, 1, 0, 0, 1, 0, 1, 1, 1, 1, 15,
                                1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 2, 1, 1, 0, 0};
  return c < 32 ? n[c] : 0;
}

static void admin(uint8_t a) {
  lastAdmin = millis();
  switch (a) {
  case 1: // reset
  case 3: // host close
    endSession();
    break;
  case 2: // host open
    hostOpen();
    break;
  case 4: // echo test
    echoNext = true;
    break;
  case 5: case 6: case 16: case 23: // historical / reserved / minor revision
    out(0);
    break;
  case 7: { // get values
    const uint8_t v[15] = {regMode, regSpeed, regTone, regWeight, regLead, regTail, regMin, regRange,
                           wk3Mode ? regX2 : regExt, regComp, regFarns, regSamp, regRatio, regPin, regX1};
    for (uint8_t b : v)
      out(b);
    break;
  }
  case 9: // firmware major revision
    out(WK_VERSION);
    break;
  case 10:
    wk2Mode = false;
    break;
  case 11:
    wk2Mode = true;
    break;
  case 12: // dump EEPROM: there is none
    for (int i = 0; i < 256; i++)
      out(0);
    break;
  case 13: // load EEPROM: swallow it
    eepromLeft = 256;
    break;
  case 14: case 15: case 22: case 25: // message, X1MODE, X2MODE, sidetone volume
    cmd = (uint16_t)(0x100 | a);
    need = 1;
    npar = 0;
    break;
  case 19: // RTTY registers
    cmd = (uint16_t)(0x100 | a);
    need = 2;
    npar = 0;
    break;
  case 20:
    wk3Mode = true;
    break;
  case 21: // supply voltage: 26214 / value = volts * 100
    out(52);
    break;
  default: // 0 calibrate, 8 debug, 17/18 baud rate
    break;
  }
}

// A buffered command or text byte, in order with the text.
static void buffered(uint8_t op, uint8_t a = 0, uint8_t b = 0) { feed.put(op, a, b); }

static void exec() {
  const uint8_t *p = par;
  bool changed = true;
  switch (cmd) {
  case 0x100 | 14: // send message
    hubPlayMemory(p[0], PROF_TX);
    changed = false;
    break;
  case 0x100 | 15:
    regX1 = p[0];
    changed = false;
    break;
  case 0x100 | 22:
    regX2 = p[0];
    changed = false;
    break;
  case 0x01: setTone(p[0]); break;
  case 0x02: setSpeed(p[0]); break;
  case 0x03: setWeight(p[0]); break;
  case 0x04: setLeadTail(p[0], p[1]); break;
  case 0x05:
    regMin = p[0];
    regRange = p[1];
    changed = false;
    break;
  case 0x06:
    feed.pause(p[0] != 0);
    changed = false;
    break;
  case 0x09: setPin(p[0]); break;
  case 0x0b:
    engineTune(p[0] ? 20000 : 0);
    changed = false;
    break;
  case 0x0d:
    regFarns = p[0];
    changed = false;
    break;
  case 0x0e: setMode(p[0]); break;
  case 0x0f: loadDefaults(p); break;
  case 0x10: setExt(p[0]); break;
  case 0x11: setComp(p[0]); break;
  case 0x12:
    regSamp = p[0];
    changed = false;
    break;
  case 0x14:
    engineSoftPaddle(p[0]);
    changed = false;
    break;
  case 0x16: // pointer commands: positions are not supported, nulls are
    if ((p[0] == 1 || p[0] == 2 || p[0] == 3) && npar == 1) {
      need = 1; // one more byte follows
      return;
    }
    changed = false;
    break;
  case 0x17: setRatio(p[0]); break;
  case 0x19: buffered(F_KEY, p[0] > 99 ? 99 : p[0]); changed = false; break;
  case 0x1a: buffered(F_WAIT, p[0] > 99 ? 99 : p[0]); changed = false; break;
  case 0x1b: {
    char a = mapIn(p[0]), b = mapIn(p[1]);
    if (a && b)
      buffered(F_RUN, (uint8_t)a, (uint8_t)b);
    changed = false;
    break;
  }
  case 0x1c: buffered(F_WPM, p[0]); changed = false; break;
  default: // 0x0c / 0x1d HSCW, 0x18 buffered PTT, admin 19 / 25 RTTY and volume
    changed = false;
    break;
  }
  cmd = 0;
  if (changed)
    applied();
}

static void command(uint8_t c) {
  switch (c) {
  case 0x07:
    out(potByte());
    return;
  case 0x08:
    feed.backspace();
    return;
  case 0x0a: // clear buffer
    feed.clear();
    engineStop();
    pendingEcho = 0;
    return;
  case 0x15:
    sendStatus(true);
    return;
  case 0x1e:
    buffered(F_WPM_RESTORE);
    return;
  case 0x13: case 0x1f: // null, buffered null
    return;
  }
  cmd = c;
  npar = 0;
  need = paramCount(c);
  if (!need)
    cmd = 0;
}

bool wkFeed(uint8_t b) {
  if (!config.winkeyer)
    return false;
  uint32_t now = millis();
  if (st == WK_OFF) {
    // 0x00 starts an admin command; loggers also send nulls (0x13) to sync.
    if (b != 0x00 && b != 0x13)
      return false;
    st = WK_CLOSED;
    dtrSeen = false;
    parserReset();
    lastAdmin = now;
    Serial.ignoreFlowControl(true); // answer even if the logger leaves DTR low
  }
  if (eepromLeft) {
    eepromLeft--;
    return true;
  }
  if (echoNext) {
    echoNext = false;
    out(b);
    return true;
  }
  if (need) {
    if (npar < sizeof(par))
      par[npar++] = b;
    if (--need == 0)
      exec();
    return true;
  }
  if (adminNext) {
    adminNext = false;
    admin(b);
    return true;
  }
  if (b == 0x00) {
    adminNext = true;
    lastAdmin = now;
    return true;
  }
  if (st != WK_OPEN) {
    // Closed: only admin commands count. Someone typing after a while
    // gets the text protocol back.
    if (b >= 0x20 && now - lastAdmin > 2000) {
      endSession();
      return false;
    }
    return true;
  }
  if (b < 0x20) {
    command(b);
    return true;
  }
  if (wasBreakin)
    return true; // the paddles have the key: text is dropped
  if (b == ';')
    buffered(F_RUN, 'A', 'A');
  else if (char c = mapIn(b))
    buffered(F_CHAR, (uint8_t)c);
  return true;
}

void wkTick() {
  if (st == WK_OFF)
    return;
  // The logger closed the port without Host Close.
  if (Serial.dtr())
    dtrSeen = true;
  else if (dtrSeen) {
    endSession();
    return;
  }
  if (st != WK_OPEN)
    return;
  uint32_t now = millis();
  bool breakin = engineKeyerBusy() || (paddleSeen && now - lastPaddle < 7u * (uint32_t)configDitMs());
  if (breakin && !wasBreakin) { // paddle break-in clears the buffer
    feed.clear();
    pendingEcho = 0;
  }
  wasBreakin = breakin;
  if (!breakin)
    feed.tick(now);
  sendStatus(false);
}

static void flushEcho() {
  if (pendingEcho)
    out((uint8_t)pendingEcho);
  pendingEcho = 0;
}

void wkEvent(const Event &e) {
  if (st != WK_OPEN)
    return;
  switch (e.type) {
  case EV_KEY:
    if (!(e.b & (EVK_PLAYER | EVK_TUNE))) {
      lastPaddle = millis();
      paddleSeen = true;
    }
    break;
  case EV_TX: // echoed once the character has been sent
    if (e.b != PROF_TX)
      break;
    flushEcho();
    if (serialEcho)
      pendingEcho = mapOut((char)e.a);
    break;
  case EV_DONE:
    flushEcho();
    break;
  case EV_ABORT:
    pendingEcho = 0;
    feed.clear();
    break;
  case EV_RX:
    if (paddleEcho && e.a) {
      char c = mapOut((char)e.a);
      if (c)
        out((uint8_t)c);
    }
    break;
  default:
    break;
  }
}

void wkButton(bool pressed) {
  if (st == WK_OPEN && wk2Mode)
    out(pressed ? 0xc9 : 0xc8);
}

void wkSpeedChanged() {
  if (st == WK_OPEN)
    out(potByte());
}
