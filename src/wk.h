#pragma once
#include "config.h"
#include "engine.h"
#include <Arduino.h>

// WinKeyer emulation on the USB serial port: contest loggers (N1MM+, DXLog,
// Win-Test, fldigi, CQRLOG, ...) drive SmartCW with the K1EL WinKeyer 2 host
// protocol. The first WinKeyer byte switches the serial port over; Host Close
// or closing the port switches it back to SmartCW's text protocol. While a
// logger has it open, the logger's settings (speed, weight, tone, ...) are
// used, and SmartCW's own come back when it closes.

// Every byte from the serial port: returns true when the WinKeyer side took it.
bool wkFeed(uint8_t b);
bool wkActive(); // the serial port speaks WinKeyer
bool wkOpen();   // a logger has the WinKeyer interface open
void wkTick();
void wkEvent(const Event &e);
void wkButton(bool pressed); // PTT button, reported as pushbutton 1 in WK2 mode
void wkSpeedChanged();       // speed set on SmartCW itself: reported as the speed pot
// config.txt gets the operator's settings, not the logger's.
void wkPersistView(Config &c);
// A setting changed on the web page or by a command (configSetHook).
void wkUserSet(const CfgItem *it);
