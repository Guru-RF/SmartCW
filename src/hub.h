#pragma once
#include <Arduino.h>

// Routes engine events to the host side: USB MIDI, the serial/TCP text
// stream, web events and USB keystrokes. Runs in the main loop.
void hubTick();

// The text stream (decoded paddle input, echo of sent text, '!' lines):
// serial port plus every TCP client.
void hubStream(const char *s);
void hubStream(char c);

// Something visible in the web UI changed (settings, beacon, modes).
void hubStateChanged();
bool hubBeaconOn();
void hubBeacon(bool on);
// Play memory 1..6. Returns false when empty or out of range.
bool hubPlayMemory(int n, uint8_t profile);
void hubPttPulse(); // PTT button / command: MIDI pulse, or Enter in keyboard mode
// Hardware check: each LED in turn, then a beep (about 2 s); "!selftest done"
// goes to the serial port and the TCP clients when it has finished.
void hubSelfTest();
