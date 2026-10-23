// Mac emulator: the few Arduino functions the audio and synth code use.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t byte;

// Emulated clock, advanced by the emulator once per audio block.
extern uint32_t emu_millis;
inline uint32_t millis() { return emu_millis; }
inline uint32_t micros() { return emu_millis * 1000; }
inline void delay(uint32_t) {}

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline long random(long max) { return max > 0 ? rand() % max : 0; }
inline long random(long min, long max) { return max > min ? min + rand() % (max - min) : min; }

#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif
