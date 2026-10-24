#pragma once
#include <Arduino.h>

// PTT push button: short press = MIDI PTT pulse (Enter in keyboard mode),
// long press (1.5 s) = toggle between straight key and the paddle keyer.
void pttBegin();
void pttTick();
void pttToggleKeyer(); // same as a long press, saved to config.txt
