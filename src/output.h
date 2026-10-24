#pragma once
#include <Arduino.h>

// Sidetone (buzzer + line out), key line and audio-mode PTT.
// Everything except outBegin()/outApplyConfig() runs in the engine interrupt.
//
// output: key   -> the key line follows transmitted CW, line out = sidetone
// output: audio -> the key line is PTT (pttLead before the first tone,
//                  pttTail after the last), line out carries the TX tone
// output: off   -> practice, key line idle, line out = sidetone
void outBegin();
void outApplyConfig(); // loop context: picked up by the interrupt on its next tick

void outSetTone(int hz);
void outKey(bool down, bool tx, uint32_t now);
void outBeep(bool on); // sidetone only, ignored while the key is down
void outTick(uint32_t now);
void outAllOff();
bool outPttActive();
bool outKeyIsDown();
