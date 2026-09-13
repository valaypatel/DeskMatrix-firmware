// emulator/smoke_test.cpp
// Standalone compile-only smoke test for Task 2: confirms the Arduino
// compatibility shim is sufficient for the real Adafruit_GFX library to
// compile and run a trivial drawing operation. No SDL2 window yet -- that
// is Task 3.
#include <Adafruit_GFX.h>
#include <cstdio>

class TestCanvas : public Adafruit_GFX {
public:
    TestCanvas() : Adafruit_GFX(8, 8) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        lastX = x; lastY = y; lastColor = color;
    }
    int16_t lastX = -1, lastY = -1;
    uint16_t lastColor = 0;
};

int main() {
    TestCanvas canvas;
    canvas.fillScreen(0);
    canvas.drawPixel(3, 4, 0xFFFF);
    if (canvas.lastX == 3 && canvas.lastY == 4 && canvas.lastColor == 0xFFFF) {
        printf("smoke test OK\n");
        return 0;
    }
    printf("smoke test FAILED\n");
    return 1;
}
