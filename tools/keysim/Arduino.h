#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
typedef std::string String;
#define F(x) x
#define INPUT_PULLUP 2
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1
extern uint32_t simNow;
inline uint32_t millis() { return simNow; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
