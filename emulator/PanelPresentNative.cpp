// emulator/PanelPresentNative.cpp
//
// Native counterpart to firmware/DeskMatrix/PanelPresent.cpp. Not compiled
// into the Arduino sketch (arduino-cli only looks inside
// firmware/DeskMatrix/) -- this is the emulator's own implementation of the
// same presentFrame()/g_activeDisplay contract declared in
// firmware/DeskMatrix/PanelPresent.h.
#include "../firmware/DeskMatrix/PanelPresent.h"
#include "NativePanel.h"

Adafruit_GFX* g_activeDisplay = nullptr;

void presentFrame(Adafruit_GFX* display) {
    static_cast<NativePanel*>(display)->present();
}
