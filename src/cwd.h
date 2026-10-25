#pragma once
#include "config.h"
#include "engine.h"
#include <Arduino.h>

// cwdaemon protocol on a UDP port of the USB network, for Linux loggers
// (CQRLOG, TLF, xlog, KLog, ...): text is sent as CW, <ESC> requests set
// speed, tone, weight and tune, and replies ("<ESC>h", "text^") come back
// once the text has been sent.
void cwdBegin(); // after the network is up
void cwdTick();
void cwdEvent(const Event &e);
void cwdPersistView(Config &c); // a '+' / '-' speed change stays out of config.txt
