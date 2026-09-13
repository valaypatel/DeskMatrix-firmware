// emulator/arduino_shim/Print.h
#pragma once
#include <cstddef>
#include <cstdint>

// Minimal stand-in for Arduino's Print base class. Adafruit_GFX inherits
// from this and implements write(uint8_t) itself (routing characters
// through drawChar()) -- this shim only needs to provide the pure virtual
// write() plus the print()/println() convenience overloads that Adafruit_GFX
// and the clockfaces actually call.
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size);

    size_t print(const char* s);
    size_t print(int v);
    size_t println(const char* s);
    size_t println();
    size_t printf(const char* format, ...);
};
