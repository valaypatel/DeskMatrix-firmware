// emulator/panel_test.cpp
// Manual visual check for Task 3: draws a red/green/blue striped pattern
// and holds the window open until closed. No DeskMatrix screen code
// involved yet -- purely verifies NativePanel itself.
#include "NativePanel.h"

int main() {
    NativePanel panel(64, 64, 8);
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            uint16_t color = 0;
            if (x < 21) color = 0xF800;       // red
            else if (x < 42) color = 0x07E0;  // green
            else color = 0x001F;              // blue
            panel.drawPixel(x, y, color);
        }
    }
    panel.present();
    while (panel.pollEvents()) {
        SDL_Delay(16);
    }
    return 0;
}
