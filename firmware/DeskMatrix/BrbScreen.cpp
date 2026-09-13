#include "screens/BrbScreen.h"

namespace {
uint16_t colorRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
}  // namespace

void drawBrbScreen(Adafruit_GFX* display) {
    display->fillScreen(0);
    display->fillScreen(colorRGB565(0, 10, 25));
    display->setTextSize(1);
    display->setTextColor(colorRGB565(80, 180, 255));
    display->setCursor(18, 28);
    display->print("BRB");
}
