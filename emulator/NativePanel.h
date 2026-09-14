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

    // Returns false once the user closes the window (checked in main.cpp's
    // loop to know when to exit).
    bool pollEvents();

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
