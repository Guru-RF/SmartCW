#pragma once
#include <Arduino.h>

// RF.Guru serial number (e.g. SCW-00042), written once on the provisioning
// bench. It lives in the last flash sector (the EEPROM area), outside the
// firmware and config.txt: firmware updates and factory settings keep it,
// only flash_nuke.uf2 erases it. Once set it is also the USB serial number.
void identBegin();         // read it and set the USB serial number; before Serial.begin()
const char *identSerial(); // "" when not set
const char *identChip();   // the RP2040's unique ID, 16 uppercase hex digits
// Store a serial number: 1-24 characters, A-Z 0-9 and '-', lowercase is
// raised. Refused while another one is set. Returns nullptr or an error.
const char *identSet(const char *s);
bool identClear();
