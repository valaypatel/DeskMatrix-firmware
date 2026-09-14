// emulator/clockface_preview.cpp
//
// Standalone preview tool: renders any clockface live through the real,
// unmodified ClockScreen.cpp/CanvasClockface.cpp, for auditioning look and
// feel without launching the full deskmatrix_emulator. Not part of the
// shipped firmware or the main emulator binary -- a development-only
// utility.
//
// Usage:
//   clockface_preview <name>              -- built-in clockface by name
//                                             (mario/words/pacman/matrix/
//                                             nyancat/starwars)
//   clockface_preview <path/to/theme.json> -- a Canvas Clockface theme JSON
//                                             file (e.g. from
//                                             jnthas/clock-club's shared/
//                                             presets), loaded as a custom
//                                             "canvas" theme
//   ... [screenshot-out.bmp]              -- optional: also dump a BMP
//                                             screenshot a few frames in
#include "NativePanel.h"
#include "NativeFS.h"
#include "EmulatorPaths.h"
#include "../firmware/DeskMatrix/PanelPresent.h"
#include "../firmware/DeskMatrix/screens/ClockScreen.h"
#include "../firmware/DeskMatrix/ConfigModel.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <set>
#include <cstdlib>

AppConfig appConfig;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <name-or-path-to-theme.json> [screenshot-out.bmp]\n";
        return 1;
    }
    const char* screenshotPath = (argc >= 3) ? argv[2] : nullptr;
    int screenshotFrame = (argc >= 4) ? std::atoi(argv[3]) : 150;

    static const std::set<std::string> kBuiltinNames = {
        "mario", "words", "pacman", "matrix", "nyancat", "starwars"};

    std::string arg = argv[1];
    if (kBuiltinNames.count(arg)) {
        appConfig.clockFace = arg;
    } else {
        std::ifstream f(arg);
        if (!f) {
            std::cerr << "Could not open " << arg << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        appConfig.canvasJson = buffer.str();
        appConfig.clockFace = "canvas";
    }

    LittleFS.setAssetsDir(resolveEmulatorPath("assets"));

    NativePanel panel(64, 64, 8);
    g_activeDisplay = &panel;

    loadClockFace(appConfig.clockFace);

    std::cout << "Previewing " << arg << " -- Escape to close\n";
    std::cout.flush();

    int frame = 0;
    while (panel.isRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            panel.handleEvent(event);
        }
        drawClockFrame(&panel);
        // Well past startup transients (e.g. procedural rain effects still
        // filling their columns) before capturing.
        if (screenshotPath && frame == screenshotFrame) {
            panel.saveScreenshotBMP(screenshotPath);
            std::cout << "Saved screenshot to " << screenshotPath << "\n";
            std::cout.flush();
        }
        frame++;
        SDL_Delay(16);
    }

    return 0;
}
