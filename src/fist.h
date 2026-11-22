#pragma once
#include <stdint.h>

// The operator's fist: four lengths the decoder learns while they key, and
// decodes against. Hand keying never has the 1:3:1:3 of a keyer: dahs run
// from 2 to 4 dits, the gap inside a letter from half a dit to two, and the
// gap between letters from barely two dits to four. One tracked value cannot
// hold that; two clusters per kind can.
//
// Marks: dit and dah lengths, the line between them halfway. Learned the way
// fldigi does, from an adjacent pair that is clearly one of each (ratio 1.7
// to 5), which also follows a change of operator within a few letters; and
// from a single mark that falls inside its cluster, for runs of one kind.
// Gaps: the same for the gap inside a letter and the gap between letters.
// Everything runs in the 1 kHz interrupt: integers, no allocation.
struct Fist {
  uint16_t dit, dah, gap, cgap; // ms
};

void fistSeed(int wpm);           // from config.fist* when set, else the keyer speed
void fistTick();                  // picks up a changed fist* setting
char fistMark(uint16_t ms);       // classifies a hand-made mark ('.' or '-') and learns from it
char fistClassify(uint16_t ms);   // the same line, without learning: a letter's marks judged again at its end
void fistKeyerMark(uint16_t ms);  // a keyer-made mark: keeps the mark clusters at the keyer's speed
void fistSpace(uint16_t ms);      // the silence before a mark (inside a word): learns the gaps
void fistWordBreak();             // a word ended: pairs do not reach across it
uint16_t fistCharEnd();           // silence after a mark that ends the letter
uint16_t fistWordEnd();           // silence after a mark that ends the word
// How surely a letter was read: its worst element as a percentage of the way
// from its cluster to the decision line (0 dead on, 100 on the line, more:
// beyond its cluster, an outlier). Hand-made marks against dit and dah; the
// gaps inside the letter against the letter gap; the gap before it against
// the gap inside a letter (the letter before may really continue in this one).
uint8_t fistFit(const char *pat, const uint16_t *ms, const uint16_t *gap, uint16_t hand, uint8_t n);
const Fist &fistNow();
bool fistSaved();                 // config.txt holds a fist
void fistSave();                  // live values into config.txt
void fistForget();                // config.txt cleared, live values back to the keyer speed
