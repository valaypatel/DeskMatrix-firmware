// firmware/DeskMatrix/PanelPresent.cpp
#include "PanelPresent.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

Adafruit_GFX* g_activeDisplay = nullptr;

void presentFrame(Adafruit_GFX* display) {
    static_cast<MatrixPanel_I2S_DMA*>(display)->flipDMABuffer();
}
