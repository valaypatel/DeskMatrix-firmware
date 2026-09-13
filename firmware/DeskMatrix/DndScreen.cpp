#include "screens/DndScreen.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

void drawDndScreen(Adafruit_GFX* display) {
    auto* panel = static_cast<MatrixPanel_I2S_DMA*>(display);
    panel->clearScreen();
    display->fillScreen(panel->color565(20, 0, 0));
    display->setTextSize(1);
    display->setTextColor(panel->color565(255, 60, 60));
    display->setCursor(18, 28);
    display->print("DND");
}
