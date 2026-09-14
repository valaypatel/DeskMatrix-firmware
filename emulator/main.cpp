// emulator/main.cpp
//
// Desktop emulator entry point. Loads fake_config.json, then runs the same
// draw*() functions the real firmware's DeskMatrix.ino loop() calls,
// switched by keyboard hotkeys instead of ScreenStateMachine/IMU/Spotify
// polling. See docs/superpowers/specs/2026-09-13-desktop-emulator-design.md
// for the full design.
#include "NativePanel.h"
#include "NativeFS.h"
#include "EmulatorPaths.h"
#include "../firmware/DeskMatrix/PanelPresent.h"
#include "../firmware/DeskMatrix/screens/ClockScreen.h"
#include "../firmware/DeskMatrix/screens/ScreensaverScreen.h"
#include "../firmware/DeskMatrix/screens/DndScreen.h"
#include "../firmware/DeskMatrix/screens/BrbScreen.h"
#include "../firmware/DeskMatrix/screens/SpotifyScreen.h"
#include "../firmware/DeskMatrix/ConfigModel.h"

#include <fstream>
#include <sstream>
#include <iostream>

// AppConfig is declared in ConfigModel.h; ClockScreen.cpp reaches for it via
// `extern AppConfig appConfig;` (see ClockScreen.cpp's loadClockFace()).
AppConfig appConfig;

enum class Scene { Clock, Screensaver, Dnd, Brb, Spotify };

int main() {
    LittleFS.setAssetsDir(resolveEmulatorPath("assets"));

    // Minimal JSON load: fake_config.json only ever has the two fields
    // below for v1, so a full ArduinoJson dependency isn't needed here --
    // ConfigModel.cpp's real parseConfig() is not reused because it also
    // pulls in ArduinoJson, which isn't part of this plan's native shim
    // scope.
    {
        std::ifstream f(resolveEmulatorPath("fake_config.json"));
        std::stringstream buffer;
        buffer << f.rdbuf();
        std::string contents = buffer.str();
        size_t pos = contents.find("\"clockFace\"");
        if (pos != std::string::npos) {
            size_t start = contents.find('"', contents.find(':', pos)) + 1;
            size_t end = contents.find('"', start);
            appConfig.clockFace = contents.substr(start, end - start);
        }
    }

    NativePanel panel(64, 64, 8);
    g_activeDisplay = &panel;

    loadClockFace(appConfig.clockFace);
    loadScreensaverGif();

    Scene scene = Scene::Clock;
    int screensaverLoops = 0;

    std::cout << "Keys: 1=Mario 2=Words 3=Pacman 4=NyanCat(preset) 5=Canvas(custom JSON)  d=DND b=BRB s=Screensaver c=Clock p=Spotify  Escape closes\n";

    // Single poll site: NativePanel::pollEvents() drains the whole SDL
    // event queue internally (fine for the other emulator test binaries,
    // which only care about SDL_QUIT/Escape), so calling it here AND then
    // running a second SDL_PollEvent loop below would find nothing left --
    // every keydown would already have been consumed and discarded. Instead
    // this loop is the only site that calls SDL_PollEvent, and it forwards
    // each event to both panel.handleEvent() (quit/Escape) and the hotkey
    // switch below, so both see every event.
    while (panel.isRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            panel.handleEvent(event);
            if (event.type != SDL_KEYDOWN) continue;
            switch (event.key.keysym.sym) {
                case SDLK_1: appConfig.clockFace = "mario"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_2: appConfig.clockFace = "words"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_3: appConfig.clockFace = "pacman"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_4: appConfig.clockFace = "nyancat"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_5:
                    // Exercises the actual "Canvas (custom)" paste-JSON path
                    // (appConfig.canvasJson), not just the built-in nyancat/
                    // starwars presets -- see loadClockFace()'s "canvas"
                    // branch in ClockScreen.cpp.
                    appConfig.canvasJson =
                        "{\"name\":\"Test\",\"bgColor\":0,\"delay\":50,"
                        "\"setup\":[{\"type\":\"text\",\"content\":\"HI\",\"x\":10,\"y\":10,\"fgColor\":65535}],"
                        "\"sprites\":[],\"loop\":[]}";
                    appConfig.clockFace = "canvas";
                    loadClockFace(appConfig.clockFace);
                    scene = Scene::Clock;
                    break;
                case SDLK_d: scene = Scene::Dnd; break;
                case SDLK_b: scene = Scene::Brb; break;
                case SDLK_s: scene = Scene::Screensaver; screensaverLoops = 0; break;
                case SDLK_c: scene = Scene::Clock; break;
                case SDLK_p: scene = Scene::Spotify; break;
                default: break;
            }
        }

        switch (scene) {
            case Scene::Clock:
                drawClockFrame(&panel);
                break;
            case Scene::Screensaver:
                drawScreensaverFrame(&panel);
                if (screensaverGifLoopCompleted()) {
                    screensaverLoops++;
                    if (screensaverLoops >= 10) scene = Scene::Clock;
                }
                break;
            case Scene::Dnd:
                // Unlike drawClockFrame()/drawScreensaverFrame()/
                // drawSpotifyScreen(), drawDndScreen() does not call
                // presentFrame() internally, so it must be presented
                // explicitly here (matching DeskMatrix.ino's loop(), which
                // calls dma_display->flipDMABuffer() right after it).
                drawDndScreen(&panel);
                presentFrame(&panel);
                break;
            case Scene::Brb:
                // Same reasoning as the Dnd case above: drawBrbScreen()
                // doesn't self-present either.
                drawBrbScreen(&panel);
                presentFrame(&panel);
                break;
            case Scene::Spotify:
                drawSpotifyScreen(&panel, "https://example.com/fake-album-art.jpg", true);
                break;
        }

        SDL_Delay(16);
    }

    return 0;
}
