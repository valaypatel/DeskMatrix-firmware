#include "screens/DndScreen.h"

void drawDndScreen(MatrixPanel_I2S_DMA* display) {
    display->clearScreen();
    display->fillScreen(display->color565(20, 0, 0));
    display->setTextSize(1);
    display->setTextColor(display->color565(255, 60, 60));
    display->setCursor(18, 28);
    display->print("DND");
}
