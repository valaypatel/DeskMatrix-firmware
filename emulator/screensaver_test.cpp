// emulator/screensaver_test.cpp
// Manual visual check for Task 4: plays emulator/assets/screensaver.gif in
// a NativePanel window using the real ScreensaverScreen.cpp code, unchanged.
#include "NativePanel.h"
#include "NativeFS.h"
#include "EmulatorPaths.h"
#include "../firmware/DeskMatrix/screens/ScreensaverScreen.h"

int main() {
    NativePanel panel(64, 64, 8);
    LittleFS.setAssetsDir(resolveEmulatorPath("assets"));
    loadScreensaverGif();
    while (panel.pollEvents()) {
        drawScreensaverFrame(&panel);
        SDL_Delay(16);
    }
    return 0;
}
