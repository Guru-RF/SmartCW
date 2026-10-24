#include "web.h"
#include "cli.h"
#include "config.h"
#include "engine.h"
#include "hub.h"
#include "net.h"
#include "pins.h"
#include "usbio.h"
#include "web_assets.h"
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <lwip/tcp.h>

// lwIP is built with ten TCP connections (__LWIP_MEMMULT=2), shared with the
// TCP text port; the page is one self-contained file and each browser tab
// keeps a single event stream open.
static const int SLOTS = 6;
static const int REQMAX = 6144;
static const int MAX_SSE = 3;

enum SlotState : uint8_t { S_FREE, S_READ, S_SEND, S_DRAIN, S_SSE };

struct Slot {
  WiFiClient c;
  SlotState st = S_FREE;
  unsigned long t = 0;
  char buf[REQMAX + 1]; // request, then the event ring for streams
  uint16_t len = 0;
  String head;          // status line, headers and small bodies
  uint32_t headOff = 0;
  const uint8_t *body = nullptr;
  uint32_t bodyLen = 0, bodyOff = 0;
  String dyn; // storage for a dynamic body
  uint16_t rh = 0, rt = 0; // event ring indices
  unsigned long ping = 0;
};

static WiFiServer *server = nullptr;
static Slot slots[SLOTS];
static bool stateDirty = false;
static unsigned long stateSentAt = 0;
static unsigned long rebootAt = 0;
static bool rebootToBoot = false;

class StrPrint : public Print {
public:
  String s;
  size_t write(uint8_t c) override {
    s += (char)c;
    return 1;
  }
};

// ---------------- helpers ----------------
static void jsonEsc(String &out, const char *s) {
  out += '"';
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", c);
      out += b;
    } else {
      out += c;
    }
  }
  out += '"';
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void urlDecode(char *s) {
  char *o = s;
  for (char *p = s; *p; p++) {
    if (*p == '+') {
      *o++ = ' ';
    } else if (*p == '%' && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) {
      *o++ = (char)(hexval(p[1]) * 16 + hexval(p[2]));
      p += 2;
    } else {
      *o++ = *p;
    }
  }
  *o = 0;
}

// Calls fn(key, value) for every pair of an url-encoded form (modifies s).
template <typename F> static void forEachParam(char *s, F fn) {
  while (s && *s) {
    char *amp = strchr(s, '&');
    if (amp)
      *amp = 0;
    char *eq = strchr(s, '=');
    const char *v = "";
    if (eq) {
      *eq = 0;
      v = eq + 1;
    }
    urlDecode(s);
    urlDecode((char *)v);
    if (*s)
      fn(s, v);
    s = amp ? amp + 1 : nullptr;
  }
}

static String param(char *form, const char *key) {
  // non-destructive lookup on a copy
  String out;
  if (!form)
    return out;
  String copy = form;
  char *buf = (char *)copy.c_str();
  forEachParam(buf, [&](const char *k, const char *v) {
    if (!strcmp(k, key))
      out = v;
  });
  return out;
}

static const char *statusText(int code) {
  switch (code) {
  case 200: return "OK";
  case 204: return "No Content";
  case 304: return "Not Modified";
  case 400: return "Bad Request";
  case 404: return "Not Found";
  case 409: return "Conflict";
  case 413: return "Payload Too Large";
  default: return "Error";
  }
}

static void respond(Slot &s, int code, const char *type, const String &body, const char *extra = nullptr) {
  s.head = "HTTP/1.1 " + String(code) + " " + statusText(code) + "\r\n";
  if (type) {
    s.head += "Content-Type: ";
    s.head += type;
    s.head += "\r\n";
  }
  s.head += "Content-Length: " + String(body.length()) + "\r\n";
  s.head += "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n";
  if (extra)
    s.head += extra;
  s.head += "\r\n";
  s.dyn = body;
  s.body = (const uint8_t *)s.dyn.c_str();
  s.bodyLen = s.dyn.length();
  s.headOff = s.bodyOff = 0;
  s.st = S_SEND;
}

static void json(Slot &s, const String &j) { respond(s, 200, "application/json", j); }
static void ok(Slot &s) { json(s, "{\"ok\":true}"); }
static void fail(Slot &s, int code, const char *msg) {
  String j = "{\"ok\":false,\"error\":";
  jsonEsc(j, msg);
  j += "}";
  respond(s, code, "application/json", j);
}

void webStateJson(String &out) {
  out.reserve(2600);
  out += "{\"v\":\"" SMARTCW_VERSION "\",\"up\":";
  out += String(millis() / 1000);
  out += ",\"cfg\":";
  configJsonValues(out);
  out += ",\"rt\":{";
  out += "\"midi\":";
  out += usbMidiActive() ? "true" : "false";
  out += ",\"kbd\":";
  out += usbKeyboardActive() ? "true" : "false";
  out += ",\"drive\":";
  out += usbDriveActive() ? "true" : "false";
  out += ",\"ip\":\"" + netIp() + "\"";
  out += ",\"beacon\":";
  out += hubBeaconOn() ? "true" : "false";
  out += ",\"practice\":";
  out += enginePracticeActive() ? "true" : "false";
  out += ",\"busy\":";
  out += enginePlayerBusy() ? "true" : "false";
  out += ",\"tune\":";
  out += engineTuning() ? "true" : "false";
  out += ",\"restart\":";
  out += configRestartNeeded() ? "true" : "false";
  out += ",\"dirty\":";
  out += configDirty() ? "true" : "false";
  out += ",\"web\":" + String(webClients());
  out += ",\"tcp\":" + String(netTcpClients());
  out += "}}";
}

int webClients() {
  int n = 0;
  for (auto &s : slots)
    if (s.st == S_SSE)
      n++;
  return n;
}

// ---------------- event streams ----------------
static bool ringPut(Slot &s, const char *p, size_t n) {
  size_t used = (size_t)((s.rh + REQMAX - s.rt) % REQMAX);
  if (used + n >= (size_t)REQMAX - 1)
    return false;
  for (size_t i = 0; i < n; i++) {
    s.buf[s.rh] = p[i];
    s.rh = (uint16_t)((s.rh + 1) % REQMAX);
  }
  return true;
}

static void sseSend(Slot &s, const char *json) {
  size_t jl = strlen(json);
  size_t used = (size_t)((s.rh + REQMAX - s.rt) % REQMAX);
  if (used + jl + 8 >= (size_t)REQMAX - 1) {
    stateDirty = true; // a slow browser: resync it with a full state later
    return;
  }
  ringPut(s, "data: ", 6);
  ringPut(s, json, jl);
  ringPut(s, "\n\n", 2);
}

void webEvent(const char *json) {
  for (auto &s : slots)
    if (s.st == S_SSE)
      sseSend(s, json);
}

void webStateDirty() { stateDirty = true; }

static void pushState() {
  String st;
  webStateJson(st);
  String ev = "{\"e\":\"state\",\"s\":";
  ev += st;
  ev += "}";
  webEvent(ev.c_str());
  stateSentAt = millis();
  stateDirty = false;
}

static void startSse(Slot &s) {
  int open = 0;
  Slot *oldest = nullptr;
  for (auto &o : slots)
    if (o.st == S_SSE) {
      open++;
      if (!oldest || o.t < oldest->t)
        oldest = &o;
    }
  if (open >= MAX_SSE && oldest) {
    oldest->c.stop();
    oldest->st = S_FREE;
  }
  s.head = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\n"
           "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\nretry: 1500\n\n";
  s.headOff = 0;
  s.body = nullptr;
  s.bodyLen = s.bodyOff = 0;
  s.rh = s.rt = 0;
  s.st = S_SSE;
  s.t = millis();
  s.ping = millis();
  s.c.setNoDelay(true);
  stateDirty = true;
}

static void flushSse(Slot &s) {
  // headers first
  while (s.headOff < s.head.length()) {
    int room = s.c.availableForWrite();
    if (room <= 0)
      return;
    size_t n = s.head.length() - s.headOff;
    if (n > (size_t)room)
      n = room;
    size_t w = s.c.write((const uint8_t *)s.head.c_str() + s.headOff, n);
    if (!w)
      return;
    s.headOff += w;
  }
  while (s.rt != s.rh) {
    int room = s.c.availableForWrite();
    if (room <= 0)
      return;
    uint16_t len = s.rh > s.rt ? (uint16_t)(s.rh - s.rt) : (uint16_t)(REQMAX - s.rt);
    if (len > room)
      len = (uint16_t)room;
    if (len > 1024)
      len = 1024;
    size_t w = s.c.write((const uint8_t *)s.buf + s.rt, len);
    if (!w)
      return;
    s.rt = (uint16_t)((s.rt + w) % REQMAX);
  }
}

// ---------------- routes ----------------
static void apiSet(Slot &s, char *form) {
  String errors;
  bool keyerChanged = false;
  forEachParam(form, [&](const char *k, const char *v) {
    const char *e = cfgSet(k, v);
    if (e) {
      if (errors.length())
        errors += ',';
      jsonEsc(errors, k);
      errors += ':';
      jsonEsc(errors, e);
    } else if (!strcasecmp(k, "keyer")) {
      keyerChanged = true;
    }
  });
  cliApplied(keyerChanged ? "keyer" : "");
  String j = "{\"ok\":";
  j += errors.length() ? "false" : "true";
  j += ",\"err\":{" + errors + "},\"restart\":";
  j += configRestartNeeded() ? "true" : "false";
  j += "}";
  json(s, j);
}

static uint8_t profileParam(char *form) {
  String p = param(form, "p");
  if (p == "train" || p == "1")
    return PROF_TRAIN;
  if (p == "practice" || p == "2")
    return PROF_PRACTICE;
  return PROF_TX;
}

static void route(Slot &s, const char *method, char *path, char *query, char *body, const char *inm) {
  bool get = !strcmp(method, "GET") || !strcmp(method, "HEAD");
  bool post = !strcmp(method, "POST");
  char *form = post ? body : query;

  if (!strcmp(method, "OPTIONS")) {
    respond(s, 204, nullptr, String(),
            "Access-Control-Allow-Methods: GET, POST\r\nAccess-Control-Allow-Headers: Content-Type\r\n");
    return;
  }
  if (get && (!strcmp(path, "/") || !strcmp(path, "/index.html"))) {
    if (inm && strstr(inm, WEB_ETAG)) {
      respond(s, 304, nullptr, String(), "ETag: \"" WEB_ETAG "\"\r\n");
      return;
    }
    s.head = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Encoding: gzip\r\n"
             "Cache-Control: no-cache\r\nETag: \"" WEB_ETAG "\"\r\nConnection: close\r\nContent-Length: ";
    s.head += String((unsigned long)WEB_INDEX_GZ_LEN);
    s.head += "\r\n\r\n";
    s.body = WEB_INDEX_GZ;
    s.bodyLen = WEB_INDEX_GZ_LEN;
    s.headOff = s.bodyOff = 0;
    s.st = S_SEND;
    if (!strcmp(method, "HEAD"))
      s.bodyLen = 0;
    return;
  }
  if (get && !strcmp(path, "/api/state")) {
    String j;
    webStateJson(j);
    json(s, j);
  } else if (get && !strcmp(path, "/api/meta")) {
    String j;
    j.reserve(6000);
    configJsonMeta(j);
    json(s, j);
  } else if (get && !strcmp(path, "/api/events")) {
    startSse(s);
  } else if (post && !strcmp(path, "/api/set")) {
    apiSet(s, form);
  } else if (post && !strcmp(path, "/api/defaults")) {
    String scope = param(form, "scope");
    StrPrint out;
    cliExec(scope == "all" ? "defaults all" : "defaults", out);
    String j;
    webStateJson(j);
    json(s, j);
  } else if (post && !strcmp(path, "/api/send")) {
    String text = param(form, "text");
    uint8_t prof = profileParam(form);
    if (prof == PROF_TRAIN) {
      String eff = param(form, "eff");
      engineTrainSpeed(param(form, "wpm").toInt(), eff.length() ? eff.toInt() : config.trainEff);
    }
    size_t n = engineQueue(text.c_str(), prof);
    json(s, "{\"ok\":true,\"queued\":" + String((unsigned)n) + ",\"free\":" + String((unsigned)engineQueueFree()) + "}");
  } else if (post && !strcmp(path, "/api/stop")) {
    engineStop();
    hubBeacon(false);
    ok(s);
  } else if (post && !strcmp(path, "/api/tune")) {
    long ms = param(form, "ms").toInt();
    engineTune(ms < 0 ? 0 : (uint32_t)ms);
    ok(s);
  } else if (post && !strcmp(path, "/api/beacon")) {
    hubBeacon(param(form, "on") == "1");
    ok(s);
  } else if (post && !strcmp(path, "/api/mem")) {
    if (hubPlayMemory(param(form, "n").toInt(), profileParam(form)))
      ok(s);
    else
      fail(s, 400, "empty memory");
  } else if (post && !strcmp(path, "/api/practice")) {
    String on = param(form, "on");
    long ms = param(form, "ms").toInt();
    if (on == "0")
      enginePractice(0);
    else
      enginePractice(ms > 0 && ms <= 600000 ? (uint32_t)ms : 8000);
    hubStateChanged();
    ok(s);
  } else if (post && !strcmp(path, "/api/ptt")) {
    hubPttPulse();
    ok(s);
  } else if (post && !strcmp(path, "/api/beep")) {
    long n = param(form, "n").toInt();
    engineBeep((uint8_t)(n < 1 ? 1 : n > 5 ? 5 : n));
    ok(s);
  } else if (post && !strcmp(path, "/api/cmd")) {
    String line = param(form, "line");
    if (line.startsWith("!"))
      line.remove(0, 1);
    StrPrint out;
    cliExec(line.c_str(), out);
    respond(s, 200, "text/plain; charset=utf-8", out.s);
  } else if (get && !strcmp(path, "/config.txt")) {
    respond(s, 200, "text/plain; charset=utf-8", configToYaml(config),
            "Content-Disposition: attachment; filename=\"smartcw-config.txt\"\r\n");
  } else if (post && !strcmp(path, "/config.txt")) {
    if (!body || !*body) {
      fail(s, 400, "empty file");
      return;
    }
    Config tmp;
    configDefaults(tmp);
    configParse(body, tmp);
    bool reboot = tmp.midi != config.midi || tmp.keyboard != config.keyboard || tmp.network != config.network ||
                  tmp.tcpPort != config.tcpPort || strcmp(tmp.ip, config.ip);
    config = tmp;
    if (reboot)
      configSetRestartNeeded();
    configMarkDirty();
    cliApplied("keyer");
    String j;
    webStateJson(j);
    json(s, j);
  } else if (post && !strcmp(path, "/api/restart")) {
    ok(s);
    rebootAt = millis() + 400;
    rebootToBoot = false;
  } else if (post && !strcmp(path, "/api/bootloader")) {
    ok(s);
    rebootAt = millis() + 400;
    rebootToBoot = true;
  } else {
    fail(s, 404, "not found");
  }
}

// Returns true when a full request is in the buffer and was handled.
static bool tryParse(Slot &s) {
  s.buf[s.len] = 0;
  char *hdrEnd = strstr(s.buf, "\r\n\r\n");
  if (!hdrEnd) {
    if (s.len >= REQMAX)
      fail(s, 413, "request too large");
    return s.len >= REQMAX;
  }
  int headerLen = (int)(hdrEnd - s.buf) + 4;
  long clen = 0;
  const char *inm = nullptr;
  char inmBuf[40] = {0};

  // header fields (case-insensitive)
  for (char *p = strstr(s.buf, "\r\n"); p && p < hdrEnd; p = strstr(p + 2, "\r\n")) {
    char *f = p + 2;
    if (!strncasecmp(f, "Content-Length:", 15))
      clen = strtol(f + 15, nullptr, 10);
    else if (!strncasecmp(f, "If-None-Match:", 14)) {
      const char *v = f + 14;
      while (*v == ' ')
        v++;
      size_t n = strcspn(v, "\r");
      if (n >= sizeof(inmBuf))
        n = sizeof(inmBuf) - 1;
      memcpy(inmBuf, v, n);
      inm = inmBuf;
    }
  }
  if (headerLen + clen > REQMAX) {
    fail(s, 413, "request too large");
    return true;
  }
  if (s.len < headerLen + clen)
    return false;

  char *body = s.buf + headerLen;
  body[clen] = 0;
  *hdrEnd = 0;

  // request line
  char *method = s.buf;
  char *sp = strchr(method, ' ');
  if (!sp) {
    fail(s, 400, "bad request");
    return true;
  }
  *sp = 0;
  char *path = sp + 1;
  sp = strchr(path, ' ');
  if (sp)
    *sp = 0;
  char *query = strchr(path, '?');
  if (query)
    *query++ = 0;
  route(s, method, path, query, body, inm);
  return true;
}

// True when more than ms passed since t (t may lie a little in the future).
static bool olderThan(unsigned long t, unsigned long ms) { return (long)(millis() - t) > (long)ms; }

static void freeSlot(Slot &s) {
  s.c.stop();
  s.st = S_FREE;
  s.head = String();
  s.dyn = String();
}

static void sendTick(Slot &s) {
  for (int guard = 0; guard < 8; guard++) {
    const uint8_t *src;
    size_t n;
    bool hdr = s.headOff < s.head.length();
    if (hdr) {
      src = (const uint8_t *)s.head.c_str() + s.headOff;
      n = s.head.length() - s.headOff;
    } else if (s.bodyOff < s.bodyLen) {
      src = s.body + s.bodyOff;
      n = s.bodyLen - s.bodyOff;
    } else {
      s.st = S_DRAIN; // wait for the browser's ACKs, then close
      s.t = millis();
      return;
    }
    int room = s.c.availableForWrite();
    int inflight = (int)TCP_SND_BUF - room;
    const int MAX_INFLIGHT = 4 * 1460; // keeps lwIP's 16 KB heap free for the others
    if (room <= 0 || inflight >= MAX_INFLIGHT) {
      return;
    }
    if (n > (size_t)(MAX_INFLIGHT - inflight))
      n = MAX_INFLIGHT - inflight;
    if (n > 1460)
      n = 1460;
    size_t w = s.c.write(src, n);
    if (!w)
      return;
    if (hdr)
      s.headOff += w;
    else
      s.bodyOff += w;
    s.t = millis();
  }
}

void webBegin() {
  server = new WiFiServer(80);
  server->begin();
}

void webTick() {
  unsigned long now = millis();
  if (rebootAt && (long)(now - rebootAt) >= 0) {
    if (configDirty() && !usbDriveActive())
      configSave();
    if (rebootToBoot)
      rp2040.rebootToBootloader();
    rp2040.reboot();
  }

  if (server) {
    WiFiClient nc = server->accept();
    if (nc) {
      Slot *slot = nullptr;
      for (auto &s : slots)
        if (s.st == S_FREE) {
          slot = &s;
          break;
        }
      if (!slot) { // reuse the oldest plain request, never an event stream
        for (auto &s : slots)
          if (s.st != S_SSE && (!slot || s.t < slot->t))
            slot = &s;
        if (slot)
          freeSlot(*slot);
      }
      if (slot) {
        slot->c = nc;
        slot->c.setTimeout(20);
        slot->st = S_READ;
        slot->len = 0;
        slot->t = now;
      } else {
        nc.stop();
      }
    }
  }

  if (stateDirty && now - stateSentAt > 150)
    pushState();

  for (auto &s : slots) {
    switch (s.st) {
    case S_FREE:
      break;
    case S_READ: {
      int n;
      while ((n = s.c.available()) > 0 && s.len < REQMAX) {
        int room = REQMAX - s.len;
        int r = s.c.read((uint8_t *)s.buf + s.len, n < room ? n : room);
        if (r <= 0)
          break;
        s.len = (uint16_t)(s.len + r);
        s.t = now;
      }
      if (s.len && tryParse(s))
        break;
      if (!s.c.connected() || olderThan(s.t, 5000))
        freeSlot(s);
      break;
    }
    case S_SEND:
      if (!s.c.connected()) {
        freeSlot(s);
        break;
      }
      sendTick(s);
      if (s.st == S_SEND && olderThan(s.t, 10000))
        freeSlot(s);
      break;
    case S_DRAIN:
      if (!s.c.connected() || s.c.availableForWrite() >= (int)TCP_SND_BUF || olderThan(s.t, 3000)) {
        freeSlot(s);
      }
      break;
    case S_SSE:
      if (!s.c.connected()) {
        freeSlot(s);
        break;
      }
      while (s.c.available() > 0) // browsers send nothing, drain anyway
        s.c.read();
      if (olderThan(s.ping, 15000)) {
        s.ping = now;
        ringPut(s, ": ping\n\n", 8);
      }
      flushSse(s);
      break;
    }
  }
}
