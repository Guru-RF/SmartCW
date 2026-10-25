#pragma once
#include <Arduino.h>

// Key type on the paddle jack.
enum KeyerMode { KM_IAMBIC_A, KM_IAMBIC_B, KM_ULTIMATIC, KM_BUG, KM_STRAIGHT };
// What the key output jack does with transmitted CW.
//   KEY   : the output keys the transceiver like a straight key
//   AUDIO : the output is PTT, the CW tone goes out on line out (MCW / FM / SSB data)
//   OFF   : practice, nothing is transmitted (sidetone only)
enum OutputMode { OUT_KEY, OUT_AUDIO, OUT_OFF };
// How CW reaches SDR software over USB MIDI.
//   KEYED   : the keyer's output as one note (key down = note on)
//   PADDLES : the raw paddle contacts as two notes, the SDR does the keying
enum MidiMode { MIDI_KEYED, MIDI_PADDLES };
// What the USB keyboard types.
//   TEXT : decoded CW as letters
//   CTRL : the paddle contacts as Left / Right Ctrl, for vBand and Vail
enum KeyboardMode { KBD_TEXT, KBD_CTRL };
// Cut numbers in contest serial numbers: 0 as T, and 9 as N.
enum CutNumbers { CUT_OFF, CUT_T, CUT_TN };

#define CFG_MEMS 6
#define CFG_TEXT 72

struct Config {
  // keyer
  int keyer;      // KeyerMode
  int wpm;        // PARIS words per minute
  int weight;     // mark/space weighting in %, 50 = standard
  int ratio;      // dah length in tenths of a dit, 30 = 3.0
  bool paddleSwap;
  bool autoSpace; // iambic: enforce the letter space after a pause
  // sound
  int tone;       // sidetone Hz
  bool buzzer;
  int volume;     // buzzer level 1..10
  int lineLevel;  // line out level 1..10
  // transmit output
  int output;     // OutputMode
  int pttLead;    // ms between PTT and the first tone (audio mode)
  int pttTail;    // ms PTT hang after the last tone (audio mode)
  int keyComp;    // ms added to every transmitted mark, taken from the space after it
  int firstExt;   // ms added to the first transmitted mark after a pause
  // text sending
  int farnsworth; // effective wpm for spacing, 0 = off
  // trainer
  int trainWpm;
  int trainEff;   // Farnsworth effective wpm, 0 = off
  int trainTone;  // 0 = same as tone
  // USB MIDI
  bool midi;
  int midiMode;   // MidiMode
  int midiCwNote;
  int midiStraightNote;
  int midiPttNote;
  int midiDitNote;
  int midiDahNote;
  // computer
  bool keyboard;  // USB keyboard interface
  int keyboardMode; // KeyboardMode
  bool winkeyer;  // answer WinKeyer host commands on the USB serial port
  // station
  char call[16];
  char name[20];
  // memories and beacon
  char mem[CFG_MEMS][CFG_TEXT];
  int nr;          // contest serial number, sent by {nr}
  int cutNumbers;  // CutNumbers, for {nr}
  char beaconText[CFG_TEXT];
  int beaconEvery; // seconds between beacon transmissions
  // network (USB Ethernet)
  bool network;
  char ip[16];
  int tcpPort;
  int cwdaemonPort; // UDP, 0 = off
};

extern Config config;

// ---- table driven access (config.txt, serial/TCP commands, web API) ----
enum CfgType : uint8_t { CT_INT, CT_BOOL, CT_ENUM, CT_STR };
enum CfgFlags : uint8_t {
  CF_REBOOT = 1, // only takes effect after a restart (USB / network layout)
  CF_KEEP = 2,   // kept by "defaults" (station, memories, beacon text)
};

struct CfgItem {
  const char *key;
  CfgType type;
  uint16_t offset;
  int32_t min, max; // int range, or max string length
  int32_t def;      // int / bool / enum index default
  const char *defStr;
  const char *opts; // enum options, '|' separated
  const char *section;
  uint8_t flags;
  const char *help;
};

int cfgCount();
const CfgItem *cfgItem(int i);
const CfgItem *cfgFind(const char *key); // accepts legacy aliases
// Set from text. Returns nullptr on success or an error message.
const char *cfgSet(const char *key, const char *value);
String cfgGet(const CfgItem *it);
const char *cfgEnumName(const CfgItem *it, int idx);

void configDefaults(Config &c, bool keepPersonal = false);
void configParse(const char *yaml, Config &c);
String configToYaml(const Config &c);
void configJsonValues(String &out); // {"wpm":18,...}
void configJsonMeta(String &out);   // [{"k":"wpm","t":"int",...},...]

int configDitMs();
// Text with {call} / {name} / {nr} / {lastnr} expanded. usedNr is set when
// the text sends the current serial number, which then has to advance.
String configExpand(const char *text, bool *usedNr = nullptr);
String configSerial(int nr); // padded to three digits, cut numbers applied

// Mount FatFS (format on failure), handle the "firmwareupdate" trigger and
// load /config.txt, writing the default file when it is missing or empty.
// Returns false when the filesystem is unusable.
bool configBegin();
bool configSave();
// Applied to a copy of the settings just before config.txt is written: lets
// temporary changes (a logger's WinKeyer session) stay out of the file.
extern void (*configSaveFilter)(Config &c);
// Called when a setting changed through cfgSet (serial, TCP, web).
extern void (*configSetHook)(const struct CfgItem *it);
unsigned long configLastWriteMs(); // duration of the last config.txt write

// Settings changed at runtime are saved automatically once things are quiet.
void configMarkDirty();
bool configDirty();
void configTick(bool engineIdle, bool driveActive);
bool configRestartNeeded();
void configSetRestartNeeded();
