// emulator/arduino_shim/Adafruit_GFX.h
// Compatibility wrapper - the real Adafruit_GFX.h has a signature mismatch
// (declares write() as void but implements it returning size_t)
#pragma once

// Forward declare the class to allow patching
class Adafruit_GFX;

// Include the real header
#include_next <Adafruit_GFX.h>

// Suppress the compiler's strict error about the signature mismatch
// by allowing Adafruit_GFX::write to return size_t despite the void declaration
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
