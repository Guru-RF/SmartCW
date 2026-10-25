#include "usbio.h"
#include <FatFS.h>
#include <FatFSUSB.h>
#include <Keyboard.h>
#include <MIDIUSB.h>

static bool midiOn = false;
static bool kbdOn = false;
static bool driveOn = false;
static volatile bool driveMounted = false;

static void onPlug(uint32_t) {
  driveMounted = true;
  FatFS.end(); // host owns the disk now
}

static void onUnplug(uint32_t) {
  // Host ejected the drive: reboot so an edited config.txt is applied.
  driveMounted = false;
  rp2040.reboot();
}

static bool driveReady(uint32_t) { return true; }

bool usbDriveBegin() {
  if (driveOn)
    return false;
  FatFSUSB.onPlug(onPlug);
  FatFSUSB.onUnplug(onUnplug);
  FatFSUSB.driveReady(driveReady);
  driveOn = FatFSUSB.begin();
  return driveOn;
}

void usbBegin(bool midi, bool keyboard, bool drive) {
  if (midi) {
    MidiUSB.setName("SmartCW");
    midiOn = MidiUSB.begin();
  }
  if (keyboard) {
    Keyboard.begin();
    kbdOn = true;
  }
  if (drive)
    usbDriveBegin();
}

bool usbMidiActive() { return midiOn; }
bool usbKeyboardActive() { return kbdOn; }
bool usbDriveActive() { return driveOn; }
bool usbDriveMounted() { return driveMounted; }

void usbMidiNote(uint8_t note, bool on) {
  if (!midiOn)
    return;
  uint8_t pkt[4];
  pkt[0] = on ? 0x09 : 0x08;          // cable 0, CIN note on/off
  pkt[1] = on ? 0x90 : 0x80;          // channel 1
  pkt[2] = note & 0x7f;
  pkt[3] = on ? 127 : 0;
  MidiUSB.writePacket(pkt);
}

void usbTypeChar(char c) {
  if (!kbdOn)
    return;
  Keyboard.write((uint8_t)c);
}

void usbKey(uint8_t key, bool down) {
  if (!kbdOn)
    return;
  if (down)
    Keyboard.press(key);
  else
    Keyboard.release(key);
}
