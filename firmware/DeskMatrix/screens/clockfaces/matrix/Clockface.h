#pragma once

#include <Adafruit_GFX.h>
#include "lib/cw-commons/IClockface.h"
#include "lib/cw-commons/CWDateTime.h"

// Procedural "digital rain" clockface (Matrix movie style): falling green
// characters across the whole panel with the time overlaid in a boxed HUD
// in the center. Unlike Mario/Words/Pacman, this doesn't use the
// cw-gfx-engine (Tile/Object/Locator) -- the rain effect is simple enough
// to draw directly with Adafruit_GFX primitives every frame, following the
// same style as DndScreen.cpp/SpotifyScreen.cpp.
class MatrixClockface : public IClockface {
  public:
    explicit MatrixClockface(Adafruit_GFX* display);
    void setup(CWDateTime* dateTime) override;
    void update() override;

  private:
    Adafruit_GFX* display_;
    CWDateTime* dateTime_;
};
