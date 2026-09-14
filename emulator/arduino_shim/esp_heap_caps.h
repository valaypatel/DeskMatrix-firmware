// emulator/arduino_shim/esp_heap_caps.h
#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 0
inline void* heap_caps_malloc(size_t size, int) { return std::malloc(size); }
