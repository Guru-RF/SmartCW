#pragma once
#include <Arduino.h>

// Text with embedded commands (speed changes, waits, key down, reply marks)
// handed to the engine one character at a time, so every command takes
// effect exactly at its place in the text. Used by the WinKeyer and cwdaemon
// protocols. Runs in the main loop.
enum FeedOp : uint8_t {
  F_CHAR,        // a: character
  F_RUN,         // a, b: two letters sent as one sign (WinKeyer merge)
  F_WPM,         // a: speed for the text that follows
  F_WPM_STEP,    // a: signed speed step (cwdaemon '+' / '-')
  F_WPM_RESTORE, // back to the speed before the first F_WPM / F_WPM_STEP
  F_WAIT,        // a: seconds of silence
  F_KEY,         // a: seconds key down
  F_MARK,        // a: id handed to onMark once everything before it is sent
};

struct FeedItem {
  uint8_t op, a, b;
};

class TxFeeder {
public:
  static const uint16_t CAP = 192;
  bool put(uint8_t op, uint8_t a = 0, uint8_t b = 0);
  bool backspace(); // drop the last character, if it is one
  void clear();     // drop everything, cancel wait, key down and speed change
  void pause(bool on) { paused = on; }
  void tick(uint32_t now);
  uint16_t count() const { return n; }
  bool waiting() const { return waitUntil != 0 || keying; }
  // The speed before a temporary change, or 0 when none is in force.
  int baseWpm() const { return speedActive ? savedWpm : 0; }
  // A new speed was set directly: the temporary change is over, keep the new one.
  void forgetSpeed() { speedActive = false; }
  void (*onMark)(uint8_t id) = nullptr;

private:
  FeedItem q[CAP];
  uint16_t head = 0, tail = 0, n = 0;
  uint32_t waitUntil = 0, keySince = 0;
  bool keying = false, paused = false, speedActive = false;
  int savedWpm = 0;
  void setWpm(int wpm);
};
