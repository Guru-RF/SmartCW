#pragma once
#include <Arduino.h>

// PTT push button: short press = MIDI PTT pulse (Enter in keyboard mode),
// long press (1.5 s) = toggle between straight key and the paddle keyer,
// held while tapping a paddle = speed down (dit) / up (dah), announced on
// the sidetone when the button is released.
void pttBegin();
void pttTick();
void pttToggleKeyer(); // same as a long press, saved to config.txt
// A paddle tapped while PTT is held: '.' one wpm slower, '-' one faster.
void pttCommand(char c);
