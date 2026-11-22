#pragma once
#include <cstdint>
extern bool simDit, simDah; // pressed
#define PIN_DIT_SIM 13
#define PIN_DAH_SIM 12
inline bool gpio_get(int pin) { // active low paddles: true = not pressed
  if (pin == PIN_DIT_SIM) return !simDit;
  if (pin == PIN_DAH_SIM) return !simDah;
  return true;
}
inline void gpio_put(int, int) {}
