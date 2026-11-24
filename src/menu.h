#pragma once
#include <Arduino.h>

// The menu on the box: hold the PTT button and squeeze both paddles. SmartCW
// answers in CW on the sidetone, nothing is transmitted and the paddles key
// nothing until the menu is left.
//   dah = next item, dit = previous: VOL, LINE, TONE, KEY, SWAP, OUT, BUZ, KBD,
//     each said by name.
//   short press: into the item, its value is said. dah/dit step it, each step
//     said and heard: the volume at the new volume, the pitch at the new pitch;
//     a switch flips on either paddle. Short press again: saved, "R".
//   long press, or 30 s of nothing: back to how it was, "SK".
// Asked for by Dave, G3LRC, who wanted a volume knob on the box.
void menuEnter();
void menuPaddle(char c); // '.' dit, '-' dah, 'b' both (ignored inside)
void menuButton();       // a short press
void menuCancel();       // a long press or the timeout
void menuTick();
bool menuActive();
