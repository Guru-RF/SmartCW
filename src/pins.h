#pragma once

// SmartCW hardware map (RP2040). Inputs are active-low with internal pull-ups,
// LEDs are active-low (sink), CW key output is active-high.
#define PIN_DIT 13      // left paddle
#define PIN_DAH 12      // right paddle
#define PIN_PTT 16      // push button
#define PIN_BUZZER 24   // PWM sidetone -> piezo
#define PIN_LINEOUT 15  // PWM sidetone -> line out
#define PIN_CWOUT 14    // key line to transceiver
#define PIN_LED_PWR 3
#define PIN_LED_DAH 8
#define PIN_LED_DIT 11
#define PIN_LED_SER 10
#define PIN_LED_SER2 7

#define SMARTCW_VERSION "2.3.0"
