// emulator/arduino_shim/Print.cpp
#include "Print.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

size_t Print::write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size; i++) n += write(buffer[i]);
    return n;
}

size_t Print::print(const char* s) {
    return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
}

size_t Print::print(int v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", v);
    return print(buf);
}

size_t Print::println(const char* s) {
    size_t n = print(s);
    n += write('\n');
    return n;
}

size_t Print::println() {
    return write('\n');
}

size_t Print::printf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return print(buf);
}
