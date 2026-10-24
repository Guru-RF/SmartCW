#pragma once
#include <Arduino.h>

// Real-time CW engine: paddle keyer (iambic A/B, ultimatic, bug, straight),
// decoder, text player, tune and beeps. It runs from a 1 kHz timer interrupt
// so USB and network traffic never disturb the keying; everything that talks
// to the host is handed to the main loop as events.

enum EventType : uint8_t {
  EV_KEY = 1, // a: 1 down / 0 up, b: flags (EVK_*)
  EV_ELEM,    // a: '.' or '-', b: mark ms, c: gap ms before it (paddle/key input)
  EV_RX,      // a: decoded character, ' ' word gap, 0 unknown
  EV_TX,      // a: character the player starts sending, b: profile
  EV_DONE,    // the player ran out of text
  EV_ABORT,   // a paddle interrupted the player
  EV_PTT,     // a: audio-mode PTT on/off
  EV_PADDLE,  // a: 'd'/'D' dit contact on/off, 'h'/'H' dah contact on/off
};
enum KeyFlags : uint16_t {
  EVK_TX = 1,       // this mark goes out on the TX path (not practice)
  EVK_STRAIGHT = 2, // manual mark from a straight key
  EVK_PLAYER = 4,   // mark from the text player
  EVK_TUNE = 8,
};

struct Event {
  uint8_t type;
  uint8_t a;
  uint16_t b;
  uint16_t c;
};

// Text profiles
enum Profile : uint8_t {
  PROF_TX = 0,       // normal: config speed, transmitted
  PROF_TRAIN = 1,    // trainer: trainer speed and tone, never transmitted
  PROF_PRACTICE = 2, // config speed, never transmitted
};

void engineBegin();
bool enginePoll(Event &e);
bool engineEventsLost(); // and clears the flag

// Queue text; "<SK>" style prosigns are sent run together. Returns the number
// of characters accepted.
size_t engineQueue(const char *text, uint8_t profile);
size_t engineQueueFree();
void engineStop();       // drop queued text, end tune, release the key
void engineResetKeyer(); // mode switch: release and drop any partial character
void engineBeep(uint8_t count);
void engineTune(uint32_t ms); // 0 stops
void enginePractice(uint32_t ms); // no TX for this long (trainer heartbeat)
// Speed for PROF_TRAIN text queued from now on; wpm 0 = trainWpm/trainEff from config.
void engineTrainSpeed(int wpm, int eff);

bool engineBusy();        // paddles, keyer, player or tune active
bool enginePlayerBusy();
bool engineKeyerBusy();
bool enginePracticeActive();
bool engineTuning();
