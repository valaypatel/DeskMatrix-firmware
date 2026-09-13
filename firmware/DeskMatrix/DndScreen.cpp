#include "screens/DndScreen.h"

namespace {
uint16_t colorRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
}  // namespace

void drawDndScreen(Adafruit_GFX* display) {
    display->fillScreen(0);
    display->fillScreen(colorRGB565(20, 0, 0));
    display->setTextSize(1);
    display->setTextColor(colorRGB565(255, 60, 60));
    display->setCursor(18, 28);
    display->print("DND");
}
