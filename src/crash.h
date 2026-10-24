#pragma once
#include <Arduino.h>

// Remembers why the previous run ended (panic message, hard fault address)
// across the watchdog reset, so it can be reported after the restart.
void crashBegin();            // call early in setup(); keeps the record
bool crashReport(Print &out); // prints "!last crash ..." when there was one
