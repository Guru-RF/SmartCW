#include "morse.h"
#include <string.h>
#include <strings.h>

struct MorseMap {
  const char *pattern;
  char ch;
};

static const MorseMap table[] = {
    {".-", 'a'},      {"-...", 'b'},   {"-.-.", 'c'},   {"-..", 'd'},
    {".", 'e'},       {"..-.", 'f'},   {"--.", 'g'},    {"....", 'h'},
    {"..", 'i'},      {".---", 'j'},   {"-.-", 'k'},    {".-..", 'l'},
    {"--", 'm'},      {"-.", 'n'},     {"---", 'o'},    {".--.", 'p'},
    {"--.-", 'q'},    {".-.", 'r'},    {"...", 's'},    {"-", 't'},
    {"..-", 'u'},     {"...-", 'v'},   {".--", 'w'},    {"-..-", 'x'},
    {"-.--", 'y'},    {"--..", 'z'},

    {".----", '1'},   {"..---", '2'},  {"...--", '3'},  {"....-", '4'},
    {".....", '5'},   {"-....", '6'},  {"--...", '7'},  {"---..", '8'},
    {"----.", '9'},   {"-----", '0'},

    {".-.-.-", '.'},  {"--..--", ','}, {"..--..", '?'}, {"-...-", '='},
    {"-....-", '-'},  {"-..-.", '/'},  {".--.-.", '@'}, {".----.", '\''},
    {".-..-.", '"'},  {"---...", ':'}, {"-.-.-.", ';'}, {"..--.-", '_'},
    {"-.--.-", ')'},  {"...-..-", '$'},

    {"-.--.", '('},   // KN
    {".-.-.", '+'},   // AR
    {".-...", '&'},   // AS
    {"...-.-", '|'},  // SK
    {"...-.", '*'},   // SN (understood)
    {"-...-.-", '^'}, // BK
    {"........", '#'} // HH, error
};

const char *morseEncode(char c) {
  if (c >= 'A' && c <= 'Z')
    c = (char)(c - 'A' + 'a');
  for (const MorseMap &m : table)
    if (m.ch == c)
      return m.pattern;
  return nullptr;
}

char morseDecode(const char *pattern) {
  for (const MorseMap &m : table)
    if (strcmp(m.pattern, pattern) == 0)
      return m.ch;
  size_t n = strlen(pattern);
  if (n >= 6 && strspn(pattern, ".") == n)
    return '#';
  return 0;
}

char morseProsign(const char *name) {
  static const struct {
    const char *name;
    char ch;
  } ps[] = {{"AR", '+'}, {"KN", '('}, {"AS", '&'}, {"SK", '|'}, {"VA", '|'}, {"SN", '*'},
            {"VE", '*'}, {"BT", '='}, {"BK", '^'}, {"HH", '#'}};
  for (auto &p : ps)
    if (!strcasecmp(p.name, name))
      return p.ch;
  return 0;
}
