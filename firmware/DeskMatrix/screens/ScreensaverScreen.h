// firmware/DeskMatrix/screens/ScreensaverScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Loads the currently-uploaded screensaver GIF from LittleFS (see
// POST /api/screensaver). Call once at boot and again right after a new
// upload. Returns false if no screensaver GIF exists yet — the caller
// should show a blank screen instead, per the design spec's error handling.
bool loadScreensaverGif();

// Advances and draws the next animation frame if this frame's display
// duration has elapsed; a cheap no-op check otherwise. Call this every main
// loop() iteration (not gated by any fixed render tick) so playback runs at
// the GIF's own frame rate rather than being throttled to once per second.
// Flips the display buffer itself whenever it actually draws a frame.
void drawScreensaverFrame(MatrixPanel_I2S_DMA* display);
