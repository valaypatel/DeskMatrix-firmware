#include "screens/BrbScreen.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

void drawBrbScreen(Adafruit_GFX* display) {
    auto* panel = static_cast<MatrixPanel_I2S_DMA*>(display);
    panel->clearScreen();
    display->fillScreen(panel->color565(0, 10, 25));
    display->setTextSize(1);
    display->setTextColor(panel->color565(80, 180, 255));
    display->setCursor(18, 28);
    display->print("BRB");
}
