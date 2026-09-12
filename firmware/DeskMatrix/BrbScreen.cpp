#include "screens/BrbScreen.h"

void drawBrbScreen(MatrixPanel_I2S_DMA* display) {
    display->clearScreen();
    display->fillScreen(display->color565(0, 10, 25));
    display->setTextSize(1);
    display->setTextColor(display->color565(80, 180, 255));
    display->setCursor(18, 28);
    display->print("BRB");
}
