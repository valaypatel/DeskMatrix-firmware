// firmware/DeskMatrix/screens/ScreensaverScreen.h
#pragma once
#include <Adafruit_GFX.h>

// Loads the currently-uploaded screensaver GIF from LittleFS (see
// POST /api/screensaver). Call once at boot and again right after a new
// upload. Returns false if no screensaver GIF exists yet — the caller
// should show a blank screen instead, per the design spec's error handling.
bool loadScreensaverGif();

// Closes the currently-playing GIF's file handle, if any. Must be called
// before overwriting /screensaver.gif on flash — LittleFS refuses to
// remove/rename a file that's still open, which otherwise makes a new
// upload silently fail to replace the old one (confirmed on real
// hardware: esp_littlefs logs "Has open FD" and the rename never happens).
void unloadScreensaverGif();

// Advances and draws the next animation frame if this frame's display
// duration has elapsed; a cheap no-op check otherwise. Call this every main
// loop() iteration (not gated by any fixed render tick) so playback runs at
// the GIF's own frame rate rather than being throttled to once per second.
// Flips the display buffer itself whenever it actually draws a frame.
void drawScreensaverFrame(Adafruit_GFX* display);

// One-shot flag: true the first time this is called after the screensaver
// GIF (not the DND GIF — they share a decoder, see ScreensaverScreen.cpp)
// has wrapped back around to its first frame since the last call. Reading it
// clears it. Used by DeskMatrix.ino's loop() to count how many full loops
// the GIF interlude has played before reverting to the clock screen.
bool screensaverGifLoopCompleted();

// True if a custom DND GIF (/dnd.gif) has been uploaded — callers use this
// to decide between it and the plain colored DND fallback (drawDndScreen).
bool dndGifExists();

// Same contract as unloadScreensaverGif(), but for /dnd.gif. Must be
// called before overwriting that file on flash.
void unloadDndGif();

// Same contract as drawScreensaverFrame(), but for /dnd.gif. Shares the
// same underlying GIF decoder as the screensaver (reloading automatically
// whenever the requested path changes) rather than using a second decoder
// instance — Screensaver and DND are never the active screen at the same
// time, so there's no need for, or RAM cost from, a separate one.
void drawDndGifFrame(Adafruit_GFX* display);
