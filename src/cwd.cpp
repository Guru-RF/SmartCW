// cwdaemon 0.13 requests, after its README and cwdaemon.c.
#include "cwd.h"
#include "cli.h"
#include "feeder.h"
#include "hub.h"
#include <WiFiUdp.h>

static WiFiUDP udp;
static bool running = false;
static TxFeeder feed;
static IPAddress peer;
static uint16_t peerPort = 0;
static char reply[80];      // sent once the text that carries it has been played
static bool replyArmed = false; // "<ESC>h": for the next text request
static bool replyQueued = false;
static bool muted = false; // "<ESC>30" turned the buzzer off

static void sendReply(const char *s) {
  if (!peerPort)
    return;
  udp.beginPacket(peer, peerPort);
  udp.write((const uint8_t *)s, strlen(s));
  udp.endPacket();
}

static void onMark(uint8_t) {
  if (!replyQueued)
    return;
  replyQueued = false;
  char b[sizeof(reply) + 3];
  snprintf(b, sizeof(b), "%s\r\n", reply);
  sendReply(b);
}

static void abortText() {
  if (replyQueued)
    sendReply("break\r\n");
  replyQueued = false;
  feed.clear();
  engineStop();
}

void cwdPersistView(Config &c) {
  if (feed.baseWpm())
    c.wpm = feed.baseWpm();
}

void cwdBegin() {
  if (!config.cwdaemonPort)
    return;
  running = udp.begin((uint16_t)config.cwdaemonPort);
  feed.onMark = onMark;
}

static void set(const char *key, const String &v) {
  if (!cfgSet(key, v.c_str()))
    cliApplied(key);
}

static void escape(char code, const char *arg) {
  long v = atol(arg);
  switch (code) {
  case '0': // reset
    replyArmed = false;
    abortText();
    break;
  case '2':
    set("wpm", String(constrain(v, 5L, 99L)));
    break;
  case '3': // 0 = sound off
    if (v == 0) {
      muted = muted || config.buzzer;
      set("buzzer", "false");
    } else {
      set("tone", String(constrain(v, 300L, 2000L)));
      if (muted)
        set("buzzer", "true");
      muted = false;
    }
    break;
  case '4':
    abortText();
    break;
  case '7': // -50..50, 0 = standard
    set("weight", String(constrain(50 + v / 2, 25L, 75L)));
    break;
  case 'c':
    engineTune((uint32_t)constrain(v, 0L, 10L) * 1000u);
    break;
  case 'd':
    set("pttLead", String(constrain(v, 0L, 50L)));
    break;
  case 'h':
    snprintf(reply, sizeof(reply), "h%s", arg);
    replyArmed = true;
    break;
  default: // 5 exit, 6 word mode, 8 device, a PTT, b SSB source, e band, f sound, g volume
    break;
  }
}

static void text(char *t) {
  bool stepped = false, caret = false;
  char *end = strchr(t, '^');
  if (end) { // "text^": play it and send it back once played
    *end = 0;
    snprintf(reply, sizeof(reply), "%s", t);
    caret = true;
  }
  for (char *p = t; *p; p++) {
    char c = *p;
    switch (c) {
    case '+':
    case '-':
      feed.put(F_WPM_STEP, (uint8_t)(int8_t)(c == '+' ? 2 : -2));
      stepped = true;
      continue;
    case '~': // half space: not supported
    case '\r':
    case '\n':
      continue;
    case '*': c = '+'; break; // AR
    case '<': c = '|'; break; // SK
    case '!': c = '*'; break; // SN
    case '>': c = '^'; break; // BK
    }
    feed.put(F_CHAR, (uint8_t)c);
  }
  if (stepped)
    feed.put(F_WPM_RESTORE);
  if (replyArmed || caret) {
    replyArmed = false;
    replyQueued = true;
    feed.put(F_MARK);
  }
}

void cwdTick() {
  if (!running)
    return;
  feed.tick(millis());
  int n = udp.parsePacket();
  if (n <= 0)
    return;
  char buf[256];
  int len = udp.read((uint8_t *)buf, sizeof(buf) - 1);
  if (len <= 0)
    return;
  buf[len] = 0;
  peer = udp.remoteIP();
  peerPort = udp.remotePort();
  if (buf[0] == 27) {
    if (len >= 2)
      escape(buf[1], buf + 2);
  } else {
    text(buf);
  }
}

void cwdEvent(const Event &e) {
  if (running && e.type == EV_ABORT && (feed.count() || replyQueued)) {
    if (replyQueued)
      sendReply("break\r\n");
    replyQueued = false;
    feed.clear();
  }
}
