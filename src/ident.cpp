#include "ident.h"
#include <EEPROM.h>
#include <USB.h>

// EEPROM layout: "RFG1", length, the characters, a check byte.
static const char MAGIC[4] = {'R', 'F', 'G', '1'};
static const size_t MAXLEN = 24;
static char serial[MAXLEN + 1];

static uint8_t check(const char *s, size_t n) {
  uint8_t c = 0xa5;
  for (size_t i = 0; i < n; i++)
    c = (uint8_t)((c << 1 | c >> 7) ^ (uint8_t)s[i]);
  return c;
}

void identBegin() {
  EEPROM.begin(256);
  serial[0] = 0;
  bool ok = true;
  for (int i = 0; i < 4; i++)
    ok = ok && EEPROM.read(i) == (uint8_t)MAGIC[i];
  size_t n = EEPROM.read(4);
  if (ok && n >= 1 && n <= MAXLEN) {
    char b[MAXLEN + 1];
    for (size_t i = 0; i < n; i++)
      b[i] = (char)EEPROM.read(5 + i);
    b[n] = 0;
    if (EEPROM.read(5 + n) == check(b, n))
      memcpy(serial, b, n + 1);
  }
  if (serial[0])
    USB.setSerialNumber(serial); // applies when Serial.begin() enumerates
}

const char *identSerial() { return serial; }
const char *identChip() { return rp2040.getChipID(); }

const char *identSet(const char *s) {
  char b[MAXLEN + 1];
  size_t n = strlen(s);
  if (n < 1 || n > MAXLEN)
    return "1 to 24 characters";
  for (size_t i = 0; i < n; i++) {
    char c = (char)toupper((unsigned char)s[i]);
    if (!isalnum((unsigned char)c) && c != '-')
      return "only A-Z, 0-9 and -";
    b[i] = c;
  }
  b[n] = 0;
  if (serial[0] && strcmp(serial, b))
    return "already set: !id clear first";
  if (!strcmp(serial, b))
    return nullptr;
  for (int i = 0; i < 4; i++)
    EEPROM.write(i, (uint8_t)MAGIC[i]);
  EEPROM.write(4, (uint8_t)n);
  for (size_t i = 0; i < n; i++)
    EEPROM.write(5 + i, (uint8_t)b[i]);
  EEPROM.write(5 + n, check(b, n));
  rp2040.wdt_reset();
  if (!EEPROM.commit())
    return "flash write failed";
  memcpy(serial, b, n + 1);
  return nullptr;
}

bool identClear() {
  if (!serial[0])
    return true;
  for (int i = 0; i < 5; i++)
    EEPROM.write(i, 0xff);
  rp2040.wdt_reset();
  if (!EEPROM.commit())
    return false;
  serial[0] = 0;
  return true;
}
