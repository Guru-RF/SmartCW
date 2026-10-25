#include "feeder.h"
#include "config.h"
#include "engine.h"
#include "hub.h"

bool TxFeeder::put(uint8_t op, uint8_t a, uint8_t b) {
  if (n >= CAP)
    return false;
  q[head] = {op, a, b};
  head = (uint16_t)((head + 1) % CAP);
  n++;
  return true;
}

bool TxFeeder::backspace() {
  if (!n)
    return false;
  uint16_t last = (uint16_t)((head + CAP - 1) % CAP);
  if (q[last].op != F_CHAR)
    return false;
  head = last;
  n--;
  return true;
}

void TxFeeder::setWpm(int wpm) {
  if (!speedActive) {
    savedWpm = config.wpm;
    speedActive = true;
  }
  config.wpm = constrain(wpm, 5, 99);
  hubStateChanged();
}

void TxFeeder::clear() {
  head = tail = n = 0;
  waitUntil = 0;
  paused = false;
  if (keying)
    engineTune(0);
  keying = false;
  if (speedActive) {
    config.wpm = savedWpm;
    speedActive = false;
    hubStateChanged();
  }
}

void TxFeeder::tick(uint32_t now) {
  if (waitUntil) {
    if ((int32_t)(now - waitUntil) < 0)
      return;
    waitUntil = 0;
  }
  if (keying) {
    // the engine starts tuning on its next tick
    if (engineTuning() || now - keySince < 5)
      return;
    keying = false;
  }
  if (paused)
    return;
  while (n) {
    const FeedItem &it = q[tail];
    // Speed changes wait until the player has taken every earlier character
    // (it fixes the timing of a character when it starts it); waits, key down
    // and marks wait until everything earlier has been sent.
    bool taken = engineQueued() == 0;
    bool sent = taken && !enginePlayerBusy();
    switch (it.op) {
    case F_CHAR: {
      if (engineQueued() >= 2)
        return;
      char s[2] = {(char)it.a, 0};
      engineQueue(s, PROF_TX);
      break;
    }
    case F_RUN: {
      if (engineQueued() >= 2)
        return;
      char s[5] = {'<', (char)it.a, (char)it.b, '>', 0};
      engineQueue(s, PROF_TX);
      break;
    }
    case F_WPM:
      if (!taken)
        return;
      setWpm(it.a);
      break;
    case F_WPM_STEP:
      if (!taken)
        return;
      setWpm(config.wpm + (int8_t)it.a);
      break;
    case F_WPM_RESTORE:
      if (!taken)
        return;
      if (speedActive) {
        config.wpm = savedWpm;
        speedActive = false;
        hubStateChanged();
      }
      break;
    case F_WAIT:
      if (!sent)
        return;
      waitUntil = (now + (uint32_t)it.a * 1000u) | 1u;
      break;
    case F_KEY:
      if (!sent)
        return;
      if (it.a) {
        engineTune((uint32_t)it.a * 1000u);
        keying = true;
        keySince = now;
      }
      break;
    case F_MARK:
      if (!sent)
        return;
      if (onMark)
        onMark(it.a);
      break;
    }
    tail = (uint16_t)((tail + 1) % CAP);
    n--;
    if (waitUntil || keying)
      return;
  }
}
