// emulator/NativePanel.h
#pragma once
#include <Adafruit_GFX.h>
#include <SDL2/SDL.h>
#include <cstdint>

// SDL2-backed Adafruit_GFX implementation: the native/desktop counterpart
// to the real MatrixPanel_I2S_DMA. Owns an in-memory RGB565 buffer sized to
// the panel and a scaled-up SDL2 window/texture for display. `present()` is
// what PanelPresent.cpp's native build calls in place of flipDMABuffer().
class NativePanel : public Adafruit_GFX {
public:
    NativePanel(int width, int height, int scale);
    ~NativePanel() override;

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void present();

    // Returns false once the user closes the window. Drains and consumes
    // the ENTIRE SDL event queue itself (only reacting to SDL_QUIT/Escape)
    // -- fine for callers (panel_test, screensaver_test, spotify_test) that
    // have no other events to react to, but wrong for a caller that also
    // needs to see keydown events for hotkeys: this method would silently
    // swallow them before the caller's own SDL_PollEvent loop ever runs.
    // main.cpp uses isRunning()/handleEvent() instead -- see those.
    bool pollEvents();

    // Single-poll-site alternative to pollEvents(), for callers (main.cpp)
    // that also need to inspect every event themselves (e.g. for hotkeys).
    // The caller runs its own `while (SDL_PollEvent(&event))` loop, passing
    // each event to handleEvent() (for quit/Escape handling) and to its own
    // switch statement (for hotkeys) -- both see every event, since neither
    // drains the queue on its own.
    bool isRunning() const { return running_; }
    void handleEvent(const SDL_Event& event);

    // Debug/dev-tooling only: dumps the current RGB565 buffer to a BMP file
    // at `path`, so a screenshot can be inspected without macOS Screen
    // Recording permission (which `screencapture` requires and this host
    // doesn't have granted). Not used by the shipped emulator/main.cpp.
    void saveScreenshotBMP(const char* path) const;

private:
    int width_;
    int height_;
    int scale_;
    uint16_t* buffer_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    bool running_ = true;
};
