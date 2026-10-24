#pragma once

// Returns the dit/dah pattern for a character (case-insensitive), or nullptr
// when the character has no Morse equivalent.
//
// Prosigns have single-character stand-ins so they survive the text stream:
//   +  AR   (  KN   &  AS   |  SK   *  SN   =  BT   ^  BK   #  error (HH)
const char *morseEncode(char c);

// Returns the (lowercase) character for a pattern, or 0 when unknown.
// Six or more dits in a row decode as '#', the error signal.
char morseDecode(const char *pattern);

// Prosign name for "<SK>" style text, e.g. "SK" -> '|'. 0 when unknown.
char morseProsign(const char *name);
