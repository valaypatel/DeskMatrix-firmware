// emulator/canvas_preview.cpp
//
// Standalone preview tool: loads a Canvas Clockface theme JSON file (the
// same format ClockScreen.cpp's "canvas" branch consumes via
// appConfig.canvasJson) from a path given on the command line, and renders
// it live through the real, unmodified CanvasClockface.cpp/ClockScreen.cpp.
// Not part of the shipped firmware or the main emulator binary -- a
// development/preview utility only, for auditioning theme JSON files
// (e.g. from jnthas/clock-club's shared/ presets) before adopting one.
#include "NativePanel.h"
#include "NativeFS.h"
#include "EmulatorPaths.h"
#include "../firmware/DeskMatrix/PanelPresent.h"
#include "../firmware/DeskMatrix/screens/ClockScreen.h"
#include "../firmware/DeskMatrix/ConfigModel.h"

#include <fstream>
#include <sstream>
#include <iostream>

AppConfig appConfig;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-theme.json> [screenshot-out.bmp]\n";
        return 1;
    }
    const char* screenshotPath = (argc >= 3) ? argv[2] : nullptr;

    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "Could not open " << argv[1] << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    appConfig.canvasJson = buffer.str();
    appConfig.clockFace = "canvas";

    LittleFS.setAssetsDir(resolveEmulatorPath("assets"));

    NativePanel panel(64, 64, 8);
    g_activeDisplay = &panel;

    loadClockFace(appConfig.clockFace);

    std::cout << "Previewing " << argv[1] << " -- Escape to close\n";
    std::cout.flush();

    int frame = 0;
    while (panel.isRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            panel.handleEvent(event);
        }
        drawClockFrame(&panel);
        // A handful of frames in, so any first-frame decode/layout settles
        // before capturing.
        if (screenshotPath && frame == 10) {
            panel.saveScreenshotBMP(screenshotPath);
            std::cout << "Saved screenshot to " << screenshotPath << "\n";
            std::cout.flush();
        }
        frame++;
        SDL_Delay(16);
    }

    return 0;
}
