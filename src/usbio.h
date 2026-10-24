#pragma once
#include <Arduino.h>

// USB composite glue: CDC serial (always), optional MIDI, HID keyboard and
// mass-storage drive. All interfaces are registered at boot in one go.
void usbBegin(bool midi, bool keyboard, bool drive);

bool usbMidiActive();
bool usbKeyboardActive();
bool usbDriveActive();   // drive interface exposed
bool usbDriveMounted();  // host currently owns the drive (FatFS is closed)

void usbMidiNote(uint8_t note, bool on);
void usbTypeChar(char c);

// Expose the drive at runtime (re-enumerates USB; the serial port drops briefly).
bool usbDriveBegin();
