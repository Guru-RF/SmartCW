#pragma once
#include <Arduino.h>

// Web interface on port 80: the single-page UI (gzipped, built from web/),
// a JSON API and a Server-Sent Events stream of everything the engine does.
void webBegin();
void webTick();
void webEvent(const char *json); // broadcast to every event stream
void webStateDirty();            // push a fresh state to the browsers soon
void webStateJson(String &out);
int webClients(); // open event streams
