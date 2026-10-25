#include "cli.h"
#include "config.h"
#include "crash.h"
#include "engine.h"
#include "hub.h"
#include "net.h"
#include "output.h"
#include "pins.h"
#include "ptt.h"
#include "usbio.h"
#include "web.h"
#include "wk.h"
#include <FatFS.h>

static CliSession serialSession(Serial);

static const char *onoff(bool b) { return b ? "on" : "off"; }

void cliHelp(Print &o) {
  o.println(F("!SmartCW " SMARTCW_VERSION " - type text to send CW, '!' prefixes a command"));
  o.println(F("!  !help                 this list"));
  o.println(F("!  !status               settings, USB and network in words"));
  o.println(F("!  !state                everything as one JSON line"));
  o.println(F("!  !get [setting]        show one or all settings"));
  o.println(F("!  !<setting> [value]    show or change a setting, e.g. !wpm 22, !tone 650"));
  o.println(F("!  !keyer iambicA|iambicB|ultimatic|bug|straight"));
  o.println(F("!  !output key|audio|off key the radio, audio + PTT, or practice"));
  o.println(F("!  !send <text>          send text (same as typing it)"));
  o.println(F("!  !mem <1..6>           send a memory ({nr} in it sends the serial number)"));
  o.println(F("!  !nr [n]               contest serial number, e.g. !nr 1 for a new contest"));
  o.println(F("!  !beacon on|off        repeat beaconText every beaconEvery seconds"));
  o.println(F("!  !tune [seconds|off]   key down to tune (default 5 s, max 20)"));
  o.println(F("!  !practice on|off      no TX for 10 minutes, or until off"));
  o.println(F("!  !ptt                  pulse PTT (MIDI note / Enter in keyboard mode)"));
  o.println(F("!  !stop                 abort text, tune and beacon"));
  o.println(F("!  !save                 write config.txt now (changes save by themselves)"));
  o.println(F("!  !defaults [all]       factory settings; 'all' also clears call, memories"));
  o.println(F("!  !net                  web address, TCP port, clients"));
  o.println(F("!  !drive                expose config.txt as a USB drive now"));
  o.println(F("!  !reboot               software reset"));
  o.println(F("!  !bootloader           reboot into the UF2 bootloader for a firmware update"));
}

static void status(Print &o) {
  o.print(F("!version "));
  o.println(F(SMARTCW_VERSION));
  o.print(F("!keyer "));
  o.print(cfgEnumName(cfgFind("keyer"), config.keyer));
  o.print(F(", wpm "));
  o.print(config.wpm);
  o.print(F(", farnsworth "));
  o.println(config.farnsworth);
  o.print(F("!tone "));
  o.print(config.tone);
  o.print(F(" Hz, buzzer "));
  o.print(onoff(config.buzzer));
  o.print(F(", volume "));
  o.println(config.volume);
  o.print(F("!output "));
  o.print(cfgEnumName(cfgFind("output"), config.output));
  if (enginePracticeActive())
    o.print(F(" (practice active)"));
  o.println();
  o.print(F("!midi "));
  o.print(onoff(config.midi));
  o.print(F(" (active "));
  o.print(onoff(usbMidiActive()));
  o.print(F(", "));
  o.print(cfgEnumName(cfgFind("midiMode"), config.midiMode));
  o.print(F(", cw "));
  o.print(config.midiCwNote);
  o.print(F(", straight "));
  o.print(config.midiStraightNote);
  o.print(F(", ptt "));
  o.print(config.midiPttNote);
  o.println(F(")"));
  o.print(F("!keyboard "));
  o.print(onoff(config.keyboard));
  o.print(F(" (active "));
  o.print(onoff(usbKeyboardActive()));
  o.print(F(", "));
  o.print(cfgEnumName(cfgFind("keyboardMode"), config.keyboardMode));
  o.println(F(")"));
  o.print(F("!winkeyer "));
  o.print(onoff(config.winkeyer));
  if (wkOpen())
    o.print(F(" (open by a logger)"));
  o.println();
  o.print(F("!drive "));
  o.print(onoff(usbDriveActive()));
  if (usbDriveMounted())
    o.print(F(" (mounted by host)"));
  o.println();
  netPrintStatus(o);
  crashReport(o);
  if (configRestartNeeded())
    o.println(F("!restart needed to apply USB/network changes (!reboot)"));
}

static void printSetting(Print &o, const CfgItem *it) {
  o.print('!');
  o.print(it->key);
  o.print(' ');
  o.println(cfgGet(it));
}

void cliApplied(const char *key) {
  outApplyConfig();
  if (!strcmp(key, "keyer"))
    engineResetKeyer();
  hubStateChanged();
}

static bool setAndReport(Print &o, const char *key, const char *value) {
  const char *err = cfgSet(key, value);
  if (err) {
    o.print(F("!err "));
    o.println(err);
    return false;
  }
  const CfgItem *it = cfgFind(key);
  cliApplied(it->key);
  o.print('!');
  o.print(it->key);
  o.print(' ');
  o.print(cfgGet(it));
  if (it->flags & CF_REBOOT)
    o.print(F(" (restart to apply: !reboot)"));
  o.println();
  return true;
}

static const char *shortcut(const char *head) {
  if (!strcasecmp(head, "swap"))
    return "paddleSwap";
  if (!strcasecmp(head, "autospace"))
    return "autoSpace";
  if (!strcasecmp(head, "level"))
    return "lineLevel";
  return head;
}

void cliExec(const char *lineIn, Print &o) {
  char line[160];
  strncpy(line, lineIn, sizeof(line) - 1);
  line[sizeof(line) - 1] = 0;
  char *head = line;
  while (*head == ' ')
    head++;
  if (!*head)
    return;
  char *arg = strchr(head, ' ');
  if (arg) {
    *arg++ = 0;
    while (*arg == ' ')
      arg++;
    for (char *e = arg + strlen(arg); e > arg && e[-1] == ' ';)
      *--e = 0;
  } else {
    arg = head + strlen(head);
  }
  String argLower = arg;
  argLower.toLowerCase();

  if (!strcasecmp(head, "help") || !strcmp(head, "?")) {
    cliHelp(o);
  } else if (!strcasecmp(head, "status")) {
    status(o);
  } else if (!strcasecmp(head, "version")) {
    o.println(F("!version " SMARTCW_VERSION));
  } else if (!strcasecmp(head, "state")) {
    String s;
    webStateJson(s);
    o.print(F("!state "));
    o.println(s);
  } else if (!strcasecmp(head, "get")) {
    if (*arg) {
      const CfgItem *it = cfgFind(shortcut(arg));
      if (it)
        printSetting(o, it);
      else
        o.println(F("!err unknown setting"));
    } else {
      for (int i = 0; i < cfgCount(); i++)
        printSetting(o, cfgItem(i));
    }
  } else if (!strcasecmp(head, "set")) {
    char *val = strchr(arg, ' ');
    if (!val) {
      o.println(F("!err set <setting> <value>"));
      return;
    }
    *val++ = 0;
    setAndReport(o, shortcut(arg), val);
  } else if (!strcasecmp(head, "send")) {
    engineQueue(arg, PROF_TX);
  } else if (!strcasecmp(head, "mem")) {
    int n = atoi(arg);
    if (hubPlayMemory(n, PROF_TX)) {
      o.print(F("!mem "));
      o.println(n);
    } else {
      o.println(F("!err mem 1..6 (empty memories are skipped)"));
    }
  } else if (!strcasecmp(head, "beacon")) {
    if (argLower == "on" || argLower == "off")
      hubBeacon(argLower == "on");
    else if (*arg) {
      o.println(F("!err beacon on|off"));
      return;
    }
    o.print(F("!beacon "));
    o.println(onoff(hubBeaconOn()));
  } else if (!strcasecmp(head, "tune")) {
    if (argLower == "off" || argLower == "0") {
      engineTune(0);
      o.println(F("!tune off"));
    } else {
      int s = *arg ? atoi(arg) : 5;
      if (s < 1 || s > 20) {
        o.println(F("!err tune 1..20 seconds"));
        return;
      }
      engineTune((uint32_t)s * 1000);
      o.print(F("!tune "));
      o.println(s);
    }
  } else if (!strcasecmp(head, "practice")) {
    if (argLower == "off")
      enginePractice(0);
    else if (argLower == "on" || !*arg)
      enginePractice(600000);
    o.print(F("!practice "));
    o.println(onoff(enginePracticeActive()));
    hubStateChanged();
  } else if (!strcasecmp(head, "ptt")) {
    hubPttPulse();
    o.println(F("!ptt"));
  } else if (!strcasecmp(head, "stop")) {
    engineStop();
    hubBeacon(false);
    o.println(F("!stop"));
  } else if (!strcasecmp(head, "save")) {
    if (usbDriveActive()) {
      o.println(F("!err drive is exposed to the host: edit config.txt there, eject, or !reboot first"));
      return;
    }
    if (configSave()) {
      o.print(F("!saved config.txt ("));
      o.print(configLastWriteMs());
      o.println(F(" ms)"));
    } else {
      o.println(F("!err could not write config.txt"));
    }
  } else if (!strcasecmp(head, "defaults")) {
    bool all = argLower == "all";
    int oldMidi = config.midi, oldKbd = config.keyboard, oldNet = config.network, oldPort = config.tcpPort,
        oldUdp = config.cwdaemonPort;
    String oldIp = config.ip;
    configDefaults(config, !all);
    if (oldMidi != config.midi || oldKbd != config.keyboard || oldNet != config.network ||
        oldPort != config.tcpPort || oldUdp != config.cwdaemonPort || oldIp != config.ip)
      configSetRestartNeeded();
    configMarkDirty();
    cliApplied("keyer");
    o.println(all ? F("!defaults loaded (everything)") : F("!defaults loaded (call, name, memories and beacon text kept)"));
  } else if (!strcasecmp(head, "perf")) {
    extern uint32_t perfMax[6];
    o.printf("!perf max us: hub %lu, cli %lu, net %lu, cfg %lu, loop %lu\n", (unsigned long)perfMax[0], (unsigned long)perfMax[1],
             (unsigned long)perfMax[2], (unsigned long)perfMax[3], (unsigned long)perfMax[4]);
    memset(perfMax, 0, sizeof(perfMax));
  } else if (!strcasecmp(head, "net")) {
    netPrintStatus(o);
    if (argLower == "tcp")
      netPrintTcp(o);
  } else if (!strcasecmp(head, "drive")) {
    if (usbDriveActive()) {
      o.println(F("!drive already on"));
      return;
    }
    o.println(F("!drive on (USB re-enumerates, reopen the serial port)"));
    Serial.flush();
    delay(100);
    usbDriveBegin();
  } else if (!strcasecmp(head, "reboot") || !strcasecmp(head, "restart")) {
    o.println(F("!rebooting"));
    if (configDirty() && !usbDriveActive())
      configSave();
    Serial.flush();
    delay(150);
    rp2040.reboot();
  } else if (!strcasecmp(head, "bootloader")) {
    o.println(F("!rebooting into UF2 bootloader"));
    Serial.flush();
    delay(150);
    rp2040.rebootToBootloader();
  } else {
    const CfgItem *it = cfgFind(shortcut(head));
    if (!it) {
      o.print(F("!err unknown command: "));
      o.println(head);
      return;
    }
    if (*arg)
      setAndReport(o, it->key, arg);
    else
      printSetting(o, it);
  }
}

void CliSession::reset() {
  inCmd = false;
  len = 0;
  psLen = 0;
  lastCr = false;
}

void CliSession::feed(char c) {
  if (inCmd) {
    if (c == '\r' || c == '\n') {
      line[len] = 0;
      inCmd = false;
      lastCr = (c == '\r');
      cliExec(line, out);
      len = 0;
    } else if (c == 0x7f || c == 0x08) {
      if (len > 0)
        len--;
    } else if (len < sizeof(line) - 1 && (uint8_t)c >= 0x20) {
      line[len++] = c;
    }
    return;
  }
  if (c == '\n' && lastCr) { // CR LF counts once
    lastCr = false;
    return;
  }
  lastCr = (c == '\r');
  if (psLen) { // collecting "<SK>"
    ps[psLen++] = c;
    if (c == '>' || psLen >= sizeof(ps) - 1 || c == ' ') {
      ps[psLen] = 0;
      engineQueue(ps, PROF_TX);
      psLen = 0;
    }
    return;
  }
  if (c == '<') {
    ps[0] = c;
    psLen = 1;
  } else if (c == '!') {
    inCmd = true;
    len = 0;
  } else if (c == '\r' || c == '\n' || c == '\t') {
    engineQueue(" ", PROF_TX);
  } else if ((uint8_t)c >= 0x20 && (uint8_t)c < 0x7f) {
    char s[2] = {c, 0};
    engineQueue(s, PROF_TX);
  }
}

void cliBegin() { serialSession.reset(); }

void cliTick() {
  int n = 0;
  while (Serial.available() && n++ < 256) {
    uint8_t b = (uint8_t)Serial.read();
    bool was = wkActive();
    if (wkFeed(b)) {
      if (!was)
        serialSession.reset(); // a logger took over the port
      continue;
    }
    serialSession.feed((char)b);
  }
}
