#pragma once
#include <Arduino.h>

void ledsBegin();
void ledPwr(bool on);
void ledDit(bool on);
void ledDah(bool on);
void ledSer(bool on);
void ledSer2(bool on);

// Non-blocking PTT acknowledge flash (~0.9 s), driven by ledsTick().
void ledsPttFlash();
void ledsTick();
