#include "hub.h"
#include "config.h"
#include "engine.h"
#include "leds.h"
#include "net.h"
#include "usbio.h"
#include "web.h"

static bool beaconOn = false;
static bool beaconSending = false;
static unsigned long beaconNext = 0;
static bool pttNoteOn = false;
static unsigned long pttNoteOffAt = 0;
static uint8_t keyNote = 0;
static bool keyNoteOn = false;

void hubStream(const char *s) {
  Serial.print(s);
  netStream(s);
}

void hubStream(char c) {
  char s[2] = {c, 0};
  hubStream(s);
}

void hubStateChanged() { webStateDirty(); }
bool hubBeaconOn() { return beaconOn; }

void hubBeacon(bool on) {
  if (on == beaconOn)
    return;
  beaconOn = on;
  beaconNext = millis();
  if (!on && beaconSending) {
    engineStop();
    beaconSending = false;
  }
  hubStateChanged();
}

bool hubPlayMemory(int n, uint8_t profile) {
  if (n < 1 || n > CFG_MEMS || !config.mem[n - 1][0])
    return false;
  String t = configExpand(config.mem[n - 1]);
  engineQueue(t.c_str(), profile);
  return true;
}

void hubPttPulse() {
  if (usbKeyboardActive()) {
    usbTypeChar('\n');
    return;
  }
  usbMidiNote((uint8_t)config.midiPttNote, true);
  pttNoteOn = true;
  pttNoteOffAt = millis() + 150;
  ledsPttFlash();
  webEvent("{\"e\":\"pulse\"}");
}

// JSON string body for one character (no quotes)
static void jsonChar(char *dst, size_t n, char c) {
  if (c == '"' || c == '\\')
    snprintf(dst, n, "\\%c", c);
  else if ((uint8_t)c < 0x20)
    snprintf(dst, n, "\\u%04x", c);
  else
    snprintf(dst, n, "%c", c);
}

static void midiKey(const Event &e) {
  if (!usbMidiActive())
    return;
  if (e.a) {
    if (config.midiMode != MIDI_KEYED || !(e.b & EVK_TX))
      return;
    keyNote = (uint8_t)((e.b & EVK_STRAIGHT) ? config.midiStraightNote : config.midiCwNote);
    usbMidiNote(keyNote, true);
    keyNoteOn = true;
  } else if (keyNoteOn) {
    usbMidiNote(keyNote, false);
    keyNoteOn = false;
  }
}

static void handle(const Event &e) {
  char buf[96], ch[8];
  switch (e.type) {
  case EV_KEY:
    midiKey(e);
    snprintf(buf, sizeof(buf), "{\"e\":\"k\",\"d\":%d,\"f\":%u}", e.a, e.b);
    webEvent(buf);
    break;
  case EV_PADDLE:
    if (usbMidiActive() && config.midiMode == MIDI_PADDLES) {
      bool dit = (e.a == 'd' || e.a == 'D');
      bool on = (e.a == 'd' || e.a == 'h');
      usbMidiNote((uint8_t)(dit ? config.midiDitNote : config.midiDahNote), on);
    }
    break;
  case EV_ELEM:
    snprintf(buf, sizeof(buf), "{\"e\":\"el\",\"v\":\"%c\",\"m\":%u,\"g\":%u}", e.a, e.b, e.c);
    webEvent(buf);
    break;
  case EV_RX: {
    char c = (char)e.a;
    if (c) {
      hubStream(c);
      if (usbKeyboardActive() && !enginePracticeActive() && (uint8_t)c < 0x80 && c != '#')
        usbTypeChar(c);
      jsonChar(ch, sizeof(ch), c);
    } else {
      hubStream("\xC2\xBF"); // ¿
      ch[0] = 0;
    }
    snprintf(buf, sizeof(buf), "{\"e\":\"rx\",\"c\":\"%s\"}", ch);
    webEvent(buf);
    break;
  }
  case EV_TX: {
    char c = (char)e.a;
    if (e.b == PROF_TX)
      hubStream(c); // echo, as the original firmware did
    jsonChar(ch, sizeof(ch), c);
    snprintf(buf, sizeof(buf), "{\"e\":\"tx\",\"c\":\"%s\",\"p\":%u}", ch, e.b);
    webEvent(buf);
    break;
  }
  case EV_DONE:
    if (beaconSending) {
      beaconSending = false;
      beaconNext = millis() + (unsigned long)config.beaconEvery * 1000UL;
    }
    webEvent("{\"e\":\"done\"}");
    break;
  case EV_ABORT:
    if (beaconOn) { // the operator took over
      beaconOn = false;
      beaconSending = false;
      hubStream("!beacon off\n");
      hubStateChanged();
    }
    webEvent("{\"e\":\"abort\"}");
    break;
  case EV_PTT:
    if (usbMidiActive() && config.output == OUT_AUDIO)
      usbMidiNote((uint8_t)config.midiPttNote, e.a != 0);
    snprintf(buf, sizeof(buf), "{\"e\":\"ptt\",\"d\":%d}", e.a);
    webEvent(buf);
    break;
  }
}

void hubTick() {
  Event e;
  int n = 0;
  while (n++ < 32 && enginePoll(e))
    handle(e);
  if (engineEventsLost())
    webStateDirty();

  unsigned long now = millis();
  if (pttNoteOn && (long)(now - pttNoteOffAt) >= 0) {
    usbMidiNote((uint8_t)config.midiPttNote, false);
    pttNoteOn = false;
  }

  if (beaconOn && !beaconSending && !engineBusy() && (long)(now - beaconNext) >= 0) {
    String t = configExpand(config.beaconText);
    if (t.length()) {
      engineQueue(t.c_str(), PROF_TX);
      beaconSending = true;
    } else {
      beaconNext = now + 1000;
    }
  }
}
