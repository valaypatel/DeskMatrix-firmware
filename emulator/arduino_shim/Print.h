// emulator/arduino_shim/Print.h
#pragma once
#include <cstddef>
#include <cstdint>

class String;

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
    size_t print(const String& s);
    size_t println(const char* s);
    size_t println(int v);
    size_t println();
    size_t printf(const char* format, ...);
};

// Minimal stand-in for Arduino's Printable interface -- ArduinoJson's
// ConverterImpl.hpp references `::Printable` in a convertToJson()
// overload it declares (never actually called by anything in this
// build, since nothing here serializes a Printable into JSON).
class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print& p) const = 0;
};

// Minimal stand-in for Arduino's Stream base class -- ArduinoJson's
// ArduinoStreamReader.hpp only needs the type to exist for its
// is_base_of<Stream, TSource> check; nothing in this build constructs a
// Stream or reads a JsonDocument from one.
class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual size_t readBytes(char* buffer, size_t length) {
        (void)buffer;
        (void)length;
        return 0;
    }
};
