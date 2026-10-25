#include "config.h"
#include <FatFS.h>
#include <stddef.h>

Config config;

static const char *CONFIG_PATH = "/config.txt";

#define OFF(field) ((uint16_t)offsetof(Config, field))
#define I(k, f, mn, mx, d, sec, fl, h) {k, CT_INT, OFF(f), mn, mx, d, nullptr, nullptr, sec, fl, h}
#define B(k, f, d, sec, fl, h) {k, CT_BOOL, OFF(f), 0, 1, d, nullptr, nullptr, sec, fl, h}
#define E(k, f, opts, d, sec, fl, h) {k, CT_ENUM, OFF(f), 0, 0, d, nullptr, opts, sec, fl, h}
#define S(k, off, len, d, sec, fl, h) {k, CT_STR, (uint16_t)(off), 0, (len) - 1, 0, d, nullptr, sec, fl, h}
#define MEM(n, d) S("mem" #n, OFF(mem) + ((n) - 1) * CFG_TEXT, CFG_TEXT, d, "memories", CF_KEEP, "memory " #n ", {call}, {name} and {nr} are filled in")

static const CfgItem items[] = {
    E("keyer", keyer, "iambicA|iambicB|ultimatic|bug|straight", KM_IAMBIC_B, "keyer", 0,
      "key type on the paddle jack; a long press (1.5 s) on PTT toggles straight"),
    I("wpm", wpm, 5, 99, 18, "keyer", 0, "keyer and text speed, words per minute"),
    I("weight", weight, 25, 75, 50, "keyer", 0, "mark/space weighting in %, 50 = standard"),
    I("ratio", ratio, 25, 45, 30, "keyer", 0, "dah length in tenths of a dit, 30 = 3.0"),
    B("paddleSwap", paddleSwap, 0, "keyer", 0, "swap the dit and dah paddles"),
    B("autoSpace", autoSpace, 0, "keyer", 0, "iambic: wait for a full letter space after a pause"),

    I("tone", tone, 300, 2000, 600, "sound", 0, "sidetone frequency in Hz"),
    B("buzzer", buzzer, 1, "sound", 0, "piezo buzzer on/off"),
    I("volume", volume, 1, 10, 10, "sound", 0, "buzzer level 1..10"),
    I("lineLevel", lineLevel, 1, 10, 10, "sound", 0, "line out level 1..10"),

    E("output", output, "key|audio|off", OUT_KEY, "output", 0,
      "key = output keys the radio, audio = output is PTT and the tone goes out on line out, off = practice"),
    I("pttLead", pttLead, 0, 500, 50, "output", 0, "audio mode: ms between PTT and the first tone"),
    I("pttTail", pttTail, 0, 3000, 400, "output", 0, "audio mode: ms PTT stays on after the last tone"),
    I("keyComp", keyComp, 0, 50, 0, "output", 0, "ms added to every transmitted dit and dah (QSK), taken from the space after it"),
    I("firstExt", firstExt, 0, 150, 0, "output", 0, "ms added to the first dit or dah after a pause, for radios that clip it"),

    I("farnsworth", farnsworth, 0, 60, 0, "text", 0, "text sending: effective wpm for the spacing, 0 = off"),

    I("trainWpm", trainWpm, 5, 60, 20, "trainer", 0, "trainer character speed, wpm"),
    I("trainEff", trainEff, 0, 60, 12, "trainer", 0, "trainer effective (Farnsworth) speed, 0 = off"),
    I("trainTone", trainTone, 0, 2000, 0, "trainer", 0, "trainer tone in Hz, 0 = same as the sidetone"),

    B("midi", midi, 1, "midi", CF_REBOOT, "USB MIDI port"),
    E("midiMode", midiMode, "keyed|paddles", MIDI_KEYED, "midi", 0,
      "keyed = keyer output as one note, paddles = raw paddle contacts as two notes"),
    I("midiCwNote", midiCwNote, 0, 127, 65, "midi", 0, "note held while the keyer or text keys down"),
    I("midiStraightNote", midiStraightNote, 0, 127, 64, "midi", 0, "note held while the straight key is closed"),
    I("midiPttNote", midiPttNote, 0, 127, 66, "midi", 0, "note pulsed by the PTT button, held during audio-mode PTT"),
    I("midiDitNote", midiDitNote, 0, 127, 67, "midi", 0, "paddles mode: dit contact"),
    I("midiDahNote", midiDahNote, 0, 127, 68, "midi", 0, "paddles mode: dah contact"),

    B("keyboard", keyboard, 0, "computer", CF_REBOOT,
      "USB keyboard (holding PTT while plugging in turns it on once)"),
    E("keyboardMode", keyboardMode, "text|ctrl", KBD_TEXT, "computer", 0,
      "text = decoded CW is typed, PTT = Enter; ctrl = paddles as Left/Right Ctrl keys for vBand and Vail"),
    B("winkeyer", winkeyer, 1, "computer", 0, "answer WinKeyer commands on the USB serial port, for contest loggers"),

    S("call", OFF(call), sizeof(Config::call), "", "station", CF_KEEP, "your callsign"),
    S("name", OFF(name), sizeof(Config::name), "", "station", CF_KEEP, "your name"),

    MEM(1, "CQ CQ CQ DE {call} {call} K"),
    MEM(2, "{call}"),
    MEM(3, "TU 5NN"),
    MEM(4, "R TNX FER QSO 73 <SK>"),
    MEM(5, "QRL?"),
    MEM(6, "{call} DE {call}"),
    I("nr", nr, 1, 9999, 1, "memories", CF_KEEP, "contest serial number, {nr} sends it and moves on, {lastnr} repeats the last one"),
    E("cutNumbers", cutNumbers, "off|t|tn", CUT_OFF, "memories", 0, "serial numbers: t = send 0 as T, tn = also 9 as N"),
    S("beaconText", OFF(beaconText), CFG_TEXT, "VVV VVV DE {call} {call}", "beacon", CF_KEEP, "beacon message"),
    I("beaconEvery", beaconEvery, 5, 3600, 60, "beacon", 0, "seconds between beacon transmissions"),

    B("network", network, 1, "network", CF_REBOOT, "USB Ethernet with the web interface and TCP port"),
    S("ip", OFF(ip), sizeof(Config::ip), "192.168.7.1", "network", CF_REBOOT,
      "SmartCW's address; the computer gets the next free one from its DHCP server (/24)"),
    I("tcpPort", tcpPort, 1, 65535, 23, "network", CF_REBOOT, "TCP port for the text/command protocol"),
    I("cwdaemonPort", cwdaemonPort, 0, 65535, 6789, "network", CF_REBOOT, "cwdaemon UDP port for Linux loggers, 0 = off"),
};

static const int ITEM_COUNT = sizeof(items) / sizeof(items[0]);

struct Alias {
  const char *from, *to;
};
static const Alias aliases[] = {{"sideFreq", "tone"}, {"usbKeyboard", "keyboard"}, {"call_sign", "call"}};

int cfgCount() { return ITEM_COUNT; }
const CfgItem *cfgItem(int i) { return (i >= 0 && i < ITEM_COUNT) ? &items[i] : nullptr; }

const CfgItem *cfgFind(const char *key) {
  for (const Alias &a : aliases)
    if (!strcasecmp(key, a.from))
      key = a.to;
  for (const CfgItem &it : items)
    if (!strcasecmp(key, it.key))
      return &it;
  return nullptr;
}

static void *fieldPtr(Config &c, const CfgItem *it) { return (uint8_t *)&c + it->offset; }

const char *cfgEnumName(const CfgItem *it, int idx) {
  static char buf[16];
  const char *p = it->opts;
  for (int i = 0; p && *p; i++) {
    const char *e = strchr(p, '|');
    size_t n = e ? (size_t)(e - p) : strlen(p);
    if (i == idx) {
      if (n >= sizeof(buf))
        n = sizeof(buf) - 1;
      memcpy(buf, p, n);
      buf[n] = 0;
      return buf;
    }
    if (!e)
      break;
    p = e + 1;
  }
  return "?";
}

static int enumIndex(const CfgItem *it, const char *v) {
  const char *p = it->opts;
  for (int i = 0; p && *p; i++) {
    const char *e = strchr(p, '|');
    size_t n = e ? (size_t)(e - p) : strlen(p);
    if (strlen(v) == n && !strncasecmp(p, v, n))
      return i;
    if (!e)
      break;
    p = e + 1;
  }
  return -1;
}

static bool parseBoolStr(const char *v, bool &out) {
  if (!strcasecmp(v, "true") || !strcmp(v, "1") || !strcasecmp(v, "on") || !strcasecmp(v, "yes")) {
    out = true;
    return true;
  }
  if (!strcasecmp(v, "false") || !strcmp(v, "0") || !strcasecmp(v, "off") || !strcasecmp(v, "no")) {
    out = false;
    return true;
  }
  return false;
}

static bool validIp(const char *s) {
  int parts = 0, val = -1;
  for (const char *p = s;; p++) {
    if (*p >= '0' && *p <= '9') {
      val = (val < 0 ? 0 : val * 10) + (*p - '0');
      if (val > 255)
        return false;
    } else if (*p == '.' || *p == 0) {
      if (val < 0)
        return false;
      parts++;
      val = -1;
      if (*p == 0)
        break;
    } else {
      return false;
    }
  }
  return parts == 4;
}

static const char *setItem(Config &c, const CfgItem *it, const char *value) {
  static char err[64];
  void *p = fieldPtr(c, it);
  switch (it->type) {
  case CT_INT: {
    char *end;
    long v = strtol(value, &end, 10);
    if (end == value || *end) {
      snprintf(err, sizeof(err), "%s needs a number", it->key);
      return err;
    }
    if (it->offset == OFF(trainTone) && v > 0 && v < 300)
      v = 300;
    if (v < it->min || v > it->max) {
      snprintf(err, sizeof(err), "%s %ld..%ld", it->key, (long)it->min, (long)it->max);
      return err;
    }
    *(int *)p = (int)v;
    return nullptr;
  }
  case CT_BOOL: {
    bool b;
    if (!parseBoolStr(value, b)) {
      snprintf(err, sizeof(err), "%s on|off", it->key);
      return err;
    }
    *(bool *)p = b;
    return nullptr;
  }
  case CT_ENUM: {
    int idx = enumIndex(it, value);
    if (idx < 0 && it->offset == OFF(keyer) && !strcasecmp(value, "iambic"))
      idx = KM_IAMBIC_B; // firmware 2.0 name
    if (idx < 0) {
      snprintf(err, sizeof(err), "%s %s", it->key, it->opts);
      for (char *q = err; *q; q++)
        if (*q == '|')
          *q = '/';
      return err;
    }
    *(int *)p = idx;
    return nullptr;
  }
  case CT_STR: {
    if ((int32_t)strlen(value) > it->max) {
      snprintf(err, sizeof(err), "%s is at most %ld characters", it->key, (long)it->max);
      return err;
    }
    if (it->offset == OFF(ip) && !validIp(value)) {
      snprintf(err, sizeof(err), "ip must look like 192.168.7.1");
      return err;
    }
    char *dst = (char *)p;
    strncpy(dst, value, it->max);
    dst[it->max] = 0;
    if (it->offset == OFF(call))
      for (char *q = dst; *q; q++)
        *q = toupper(*q);
    return nullptr;
  }
  }
  return "bad type";
}

const char *cfgSet(const char *key, const char *value) {
  const CfgItem *it = cfgFind(key);
  if (!it)
    return "unknown setting";
  String before = cfgGet(it);
  const char *e = setItem(config, it, value);
  if (e)
    return e;
  if (cfgGet(it) != before) {
    configMarkDirty();
    if (it->flags & CF_REBOOT)
      configSetRestartNeeded();
    if (configSetHook)
      configSetHook(it);
  }
  return nullptr;
}

static String itemValue(const Config &c, const CfgItem *it) {
  const void *p = (const uint8_t *)&c + it->offset;
  switch (it->type) {
  case CT_INT:
    return String(*(const int *)p);
  case CT_BOOL:
    return *(const bool *)p ? "true" : "false";
  case CT_ENUM:
    return cfgEnumName(it, *(const int *)p);
  case CT_STR:
    return String((const char *)p);
  }
  return String();
}

String cfgGet(const CfgItem *it) { return itemValue(config, it); }

void configDefaults(Config &c, bool keepPersonal) {
  for (const CfgItem &it : items) {
    if (keepPersonal && (it.flags & CF_KEEP))
      continue;
    void *p = fieldPtr(c, &it);
    switch (it.type) {
    case CT_INT:
    case CT_ENUM:
      *(int *)p = it.def;
      break;
    case CT_BOOL:
      *(bool *)p = it.def != 0;
      break;
    case CT_STR:
      strncpy((char *)p, it.defStr, it.max);
      ((char *)p)[it.max] = 0;
      break;
    }
  }
}

void configParse(const char *yaml, Config &cfg) {
  String line;
  while (*yaml) {
    line = "";
    while (*yaml && *yaml != '\n')
      line += *yaml++;
    if (*yaml == '\n')
      yaml++;

    line.trim();
    if (line.length() == 0 || line.startsWith("#"))
      continue;
    int colon = line.indexOf(':');
    if (colon <= 0)
      continue;
    String key = line.substring(0, colon);
    String value = line.substring(colon + 1);
    key.trim();
    value.trim();

    if (value.startsWith("\"")) {
      // quoted: everything up to the closing quote, '#' allowed inside
      int end = value.indexOf('"', 1);
      value = end > 0 ? value.substring(1, end) : value.substring(1);
    } else {
      int hash = value.indexOf('#');
      if (hash >= 0)
        value = value.substring(0, hash);
      value.trim();
    }

    const CfgItem *it = cfgFind(key.c_str());
    if (it)
      setItem(cfg, it, value.c_str()); // invalid values keep the default
  }
}

static const char *sectionTitle(const char *s) {
  if (!strcmp(s, "keyer")) return "Keyer";
  if (!strcmp(s, "sound")) return "Sidetone";
  if (!strcmp(s, "output")) return "Transmit output";
  if (!strcmp(s, "text")) return "Text sending";
  if (!strcmp(s, "trainer")) return "Trainer";
  if (!strcmp(s, "midi")) return "USB MIDI (restart after changing midi)";
  if (!strcmp(s, "computer")) return "Computer (restart after turning keyboard on or off)";
  if (!strcmp(s, "station")) return "Station";
  if (!strcmp(s, "memories")) return "Memories, <SK> <KN> <AR> <BT> send prosigns, {nr} a serial number";
  if (!strcmp(s, "beacon")) return "Beacon";
  if (!strcmp(s, "network")) return "USB Ethernet (restart after changing)";
  return s;
}

String configToYaml(const Config &c) {
  String s;
  s.reserve(4096);
  s += "# SmartCW " "- iambic keyer, CW trainer and CW interface\n";
  s += "# Web interface: http://192.168.7.1 (or the ip below) over the USB cable.\n";
  s += "# Edit, save, eject the drive: the device reboots and applies the file.\n";
  s += "# Replace the whole file with the single word  firmwareupdate  to enter\n";
  s += "# UF2 bootloader mode on the next boot.\n";
  const char *section = "";
  for (const CfgItem &it : items) {
    if (strcmp(section, it.section)) {
      section = it.section;
      s += "\n# --- ";
      s += sectionTitle(section);
      s += " ---\n";
    }
    s += "# ";
    s += it.help;
    if (it.type == CT_INT) {
      s += " (" + String(it.min) + ".." + String(it.max) + ")";
    } else if (it.type == CT_ENUM) {
      String o = it.opts;
      o.replace("|", ", ");
      s += " (" + o + ")";
    }
    s += "\n";
    s += it.key;
    s += ": ";
    String v = itemValue(c, &it);
    if (it.type == CT_STR) {
      v.replace("\"", "'");
      s += "\"" + v + "\"";
    } else {
      s += v;
    }
    s += "\n";
  }
  return s;
}

static void jsonStr(String &out, const char *s) {
  out += '"';
  for (const char *p = s; *p; p++) {
    char ch = *p;
    if (ch == '"' || ch == '\\') {
      out += '\\';
      out += ch;
    } else if ((uint8_t)ch < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", ch);
      out += b;
    } else {
      out += ch;
    }
  }
  out += '"';
}

void configJsonValues(String &out) {
  out += '{';
  for (int i = 0; i < ITEM_COUNT; i++) {
    const CfgItem &it = items[i];
    if (i)
      out += ',';
    jsonStr(out, it.key);
    out += ':';
    String v = itemValue(config, &it);
    if (it.type == CT_INT)
      out += v;
    else if (it.type == CT_BOOL)
      out += v;
    else
      jsonStr(out, v.c_str());
  }
  out += '}';
}

void configJsonMeta(String &out) {
  static const char *types[] = {"int", "bool", "enum", "str"};
  out += '[';
  for (int i = 0; i < ITEM_COUNT; i++) {
    const CfgItem &it = items[i];
    if (i)
      out += ',';
    out += "{\"k\":";
    jsonStr(out, it.key);
    out += ",\"t\":\"";
    out += types[it.type];
    out += "\",\"s\":";
    jsonStr(out, it.section);
    out += ",\"h\":";
    jsonStr(out, it.help);
    if (it.type == CT_INT || it.type == CT_STR) {
      out += ",\"min\":" + String(it.min) + ",\"max\":" + String(it.max);
    }
    out += ",\"d\":";
    if (it.type == CT_STR)
      jsonStr(out, it.defStr);
    else if (it.type == CT_BOOL)
      out += it.def ? "true" : "false";
    else if (it.type == CT_ENUM)
      jsonStr(out, cfgEnumName(&it, it.def));
    else
      out += String(it.def);
    if (it.type == CT_ENUM) {
      out += ",\"o\":";
      jsonStr(out, it.opts);
    }
    if (it.flags & CF_REBOOT)
      out += ",\"r\":1";
    if (it.flags & CF_KEEP)
      out += ",\"keep\":1";
    out += '}';
  }
  out += ']';
}

int configDitMs() {
  int wpm = config.wpm > 0 ? config.wpm : 18;
  return 1200 / wpm;
}

String configSerial(int nr) {
  char b[8];
  snprintf(b, sizeof(b), "%03d", nr);
  for (char *p = b; *p; p++) {
    if (*p == '0' && config.cutNumbers != CUT_OFF)
      *p = 'T';
    else if (*p == '9' && config.cutNumbers == CUT_TN)
      *p = 'N';
  }
  return String(b);
}

String configExpand(const char *text, bool *usedNr) {
  String s = text;
  s.replace("{call}", config.call);
  s.replace("{CALL}", config.call);
  s.replace("{name}", config.name);
  s.replace("{NAME}", config.name);
  if (s.indexOf('{') >= 0) {
    String last = configSerial(config.nr > 1 ? config.nr - 1 : 1);
    s.replace("{lastnr}", last);
    s.replace("{LASTNR}", last);
    if (s.indexOf("{nr}") >= 0 || s.indexOf("{NR}") >= 0) {
      String nr = configSerial(config.nr);
      s.replace("{nr}", nr);
      s.replace("{NR}", nr);
      if (usedNr)
        *usedNr = true;
    }
  }
  return s;
}

static unsigned long lastWriteMs = 0;
static bool dirty = false;
static unsigned long dirtyAt = 0;
static bool restartNeeded = false;

static bool writeFile(const char *path, const String &data) {
  unsigned long t0 = millis();
  rp2040.wdt_reset();
  File f = FatFS.open(path, "w");
  if (!f)
    return false;
  size_t w = f.write((const uint8_t *)data.c_str(), data.length());
  rp2040.wdt_reset();
  f.close();
  rp2040.wdt_reset();
  lastWriteMs = millis() - t0;
  return w == data.length();
}

unsigned long configLastWriteMs() { return lastWriteMs; }

bool configBegin() {
  configDefaults(config);

  bool fsOk = FatFS.begin();
  if (!fsOk) {
    Serial.println("[fs] mount failed - formatting (5s to abort via reset)...");
    Serial.flush();
    delay(5000);
    if (FatFS.format() && FatFS.begin()) {
      Serial.println("[fs] formatted & mounted");
      fsOk = true;
    } else {
      Serial.println("[fs] unrecoverable");
      return false;
    }
  }
  // Write the drive label only when it differs: one flash write less per boot.
  char label[24] = {0};
  fatfs::DWORD serial = 0;
  if (fatfs::f_getlabel("", label, &serial) != fatfs::FR_OK || strcmp(label, "SMARTCW"))
    fatfs::f_setlabel("SMARTCW");

  String content;
  if (FatFS.exists(CONFIG_PATH)) {
    File f = FatFS.open(CONFIG_PATH, "r");
    if (f) {
      content.reserve(f.size() + 1);
      while (f.available())
        content += (char)f.read();
      f.close();
    }
  }
  content.trim();

  if (content == "firmwareupdate") {
    Serial.println("[cfg] firmware update requested - rebooting to UF2 bootloader");
    FatFS.remove(CONFIG_PATH);
    delay(300);
    rp2040.rebootToBootloader();
    while (true)
      ;
  }

  if (content.length() == 0) {
    Serial.println("[cfg] config.txt missing/empty - writing default");
    if (!writeFile(CONFIG_PATH, configToYaml(config)))
      Serial.println("[cfg] ERR: could not write config.txt");
    return true;
  }

  configParse(content.c_str(), config);
  // Older files lack the newer keys: rewrite once so they show up.
  if (content.indexOf("cwdaemonPort") < 0)
    writeFile(CONFIG_PATH, configToYaml(config));
  Serial.println("[cfg] config.txt loaded");
  return true;
}

void (*configSaveFilter)(Config &c) = nullptr;
void (*configSetHook)(const CfgItem *it) = nullptr;

bool configSave() {
  Config c = config;
  if (configSaveFilter)
    configSaveFilter(c);
  bool ok = writeFile(CONFIG_PATH, configToYaml(c));
  if (ok)
    dirty = false;
  return ok;
}

void configMarkDirty() {
  dirty = true;
  dirtyAt = millis();
}

bool configDirty() { return dirty; }

void configTick(bool engineIdle, bool driveActive) {
  if (!dirty || driveActive || !engineIdle)
    return;
  if (millis() - dirtyAt < 2000)
    return;
  configSave();
}

bool configRestartNeeded() { return restartNeeded; }
void configSetRestartNeeded() { restartNeeded = true; }
