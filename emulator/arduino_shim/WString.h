// emulator/arduino_shim/WString.h
#pragma once
#include <string>
#include <cstdio>

// Minimal stand-in for Arduino's String class -- just enough for
// MarioBlock.cpp (`String(_dateTime->getHour())`, `.length()`) and
// CWDateTime.cpp (`String(buffer)`, returned by value).
class String {
public:
    String() = default;
    String(const char* s) : s_(s ? s : "") {}
    String(int v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v);
        s_ = buf;
    }
    size_t length() const { return s_.length(); }
    const char* c_str() const { return s_.c_str(); }

    // Needed by ArduinoJson's ArduinoStringWriter (see
    // Serialization/Writers/ArduinoStringWriter.hpp) when serializing a
    // JsonDocument into a String -- CanvasClockface.h pulls this in
    // transitively via ClockScreen.cpp's canvas clockface support.
    bool concat(const char* s) {
        if (s) s_ += s;
        return true;
    }
    bool concat(const std::string& s) {
        s_ += s;
        return true;
    }

private:
    std::string s_;
};
