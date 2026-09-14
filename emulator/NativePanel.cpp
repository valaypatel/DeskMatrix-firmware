// emulator/NativePanel.cpp
#include "NativePanel.h"
#include <cstdlib>
#include <cstring>

NativePanel::NativePanel(int width, int height, int scale)
    : Adafruit_GFX(width, height), width_(width), height_(height), scale_(scale) {
    buffer_ = static_cast<uint16_t*>(std::calloc(width_ * height_, sizeof(uint16_t)));

    SDL_Init(SDL_INIT_VIDEO);
    window_ = SDL_CreateWindow("DeskMatrix Emulator", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width_ * scale_, height_ * scale_,
                                SDL_WINDOW_SHOWN);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING, width_, height_);
}

NativePanel::~NativePanel() {
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
    std::free(buffer_);
}

void NativePanel::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    buffer_[y * width_ + x] = color;
}

void NativePanel::present() {
    SDL_UpdateTexture(texture_, nullptr, buffer_, width_ * sizeof(uint16_t));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

bool NativePanel::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) running_ = false;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running_ = false;
    }
    return running_;
}

void NativePanel::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) running_ = false;
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running_ = false;
}
