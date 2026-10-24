#pragma once
#include <Arduino.h>

// Text protocol shared by the USB serial port and the TCP port:
// plain characters are sent as CW (and echoed), decoded paddle input comes
// back as characters. A line starting with '!' is a command; every response
// line starts with '!' too.
class CliSession {
public:
  explicit CliSession(Print &out) : out(out) {}
  void feed(char c);
  void reset();
  Print &out;

private:
  bool inCmd = false;
  bool lastCr = false;
  char line[160];
  uint8_t len = 0;
  char ps[10]; // "<SK>" prosign being typed
  uint8_t psLen = 0;
};

void cliBegin();
void cliTick(); // the serial port
// Run one command line (without the leading '!'), responses go to out.
void cliExec(const char *line, Print &out);
void cliHelp(Print &out);
// After a setting changed through any channel.
void cliApplied(const char *key);
