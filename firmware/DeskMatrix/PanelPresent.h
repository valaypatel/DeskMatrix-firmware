// firmware/DeskMatrix/PanelPresent.h
#pragma once
#include <Adafruit_GFX.h>

// The currently-active display. Set once by DeskMatrix.ino's initPanel()
// (hardware) or emulator/main.cpp (native), after the concrete display
// object is constructed. ClockScreen.cpp's loadClockFace() checks this
// instead of depending on the ESP32-specific MatrixPanel_I2S_DMA type
// directly, so it has no hardware-specific dependency beyond Adafruit_GFX.
extern Adafruit_GFX* g_activeDisplay;

// Presents whatever has been drawn into `display` to the physical/virtual
// screen. On hardware this is MatrixPanel_I2S_DMA::flipDMABuffer(); on the
// native/SDL2 target (see emulator/NativePanel.cpp) it uploads the pixel
// buffer to the window and renders it. Every shared screen .cpp file calls
// this instead of `display->flipDMABuffer()` directly, since
// flipDMABuffer() isn't part of Adafruit_GFX's portable interface.
void presentFrame(Adafruit_GFX* display);
