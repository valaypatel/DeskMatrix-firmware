// emulator/arduino_shim/LittleFS.h
//
// ScreensaverScreen.cpp (unmodified firmware source) does `#include
// <LittleFS.h>` to get the `LittleFS` global and `File` class it calls
// throughout. There's no real LittleFS.h off-ESP32, so this redirects to
// the native shim (emulator/NativeFS.h, Task 4) that implements the same
// call surface (LittleFS.open()/.exists(), File::size()/read()/seek()/
// position()/close()/operator bool()) against emulator/assets/ instead of
// flash.
#pragma once
#include "NativeFS.h"
