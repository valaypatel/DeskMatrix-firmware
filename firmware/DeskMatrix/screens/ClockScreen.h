// firmware/DeskMatrix/screens/ClockScreen.h
#pragma once
#include <Adafruit_GFX.h>
#include <string>

// Idle-screen clock, ported from Clockwise (github.com/jnthas/clockwise) —
// see lib/cw-gfx-engine, lib/cw-commons, and screens/clockfaces/{mario,
// words,pacman}. Shown as the default idle content in place of a plain GIF;
// the GIF screensaver now only interrupts it periodically (see
// DeskMatrix.ino's loop()).

// Constructs/selects the chosen clockface implementation ("mario" | "words"
// | "pacman", default "mario" for any unrecognized value) and calls its
// setup(). Call once at boot (after appConfig is loaded) and again whenever
// ConfigServer reports a clockFace change, mirroring loadScreensaverGif()'s
// pattern. Safe to call repeatedly with the same name (no-op).
void loadClockFace(const std::string& name);

// Advances and draws the active clockface, then flips the display buffer.
// Not gated to any fixed render tick, same reasoning as
// drawScreensaverFrame()/drawSpotifyScreen(): each clockface paces its own
// animation (e.g. Mario's jump) off its own millis() bookkeeping internally.
void drawClockFrame(Adafruit_GFX* display);
