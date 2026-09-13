// emulator/arduino_shim/Arduino.h
//
// Minimal Arduino-compatibility shim for the native/desktop emulator
// build. Provides only what Adafruit_GFX, AnimatedGIF, CWDateTime, and the
// DeskMatrix clockfaces actually need -- see
// docs/superpowers/specs/2026-09-13-desktop-emulator-design.md for the
// full survey this was derived from. Adafruit_GFX.cpp itself already
// falls back to portable pgm_read_byte/word/dword macros when none are
// defined (see its own #ifndef guards), so this shim deliberately leaves
// those undefined rather than duplicating that fallback.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <cmath>

#define ARDUINO 10819

#define PROGMEM
#define radians(deg) ((deg) * M_PI / 180.0)
#define degrees(rad) ((rad) * 180.0 / M_PI)

// FlashStringHelper for F() macro support
struct __FlashStringHelper;

inline unsigned long millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<unsigned long>(
        duration_cast<milliseconds>(steady_clock::now() - start).count());
}

inline long random(long max) { return max > 0 ? std::rand() % max : 0; }
inline void randomSeed(unsigned long seed) { std::srand(static_cast<unsigned>(seed)); }

#include "Print.h"
#include "WString.h"

class HardwareSerialStub : public Print {
public:
    size_t write(uint8_t c) override {
        putchar(c);
        return 1;
    }
};
inline HardwareSerialStub Serial;
