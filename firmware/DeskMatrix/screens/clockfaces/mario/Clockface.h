#pragma once

#include <Arduino.h>

#include "gfx/Super_Mario_Bros__24pt7b.h"

#include <Adafruit_GFX.h>
#include "lib/cw-gfx-engine/Tile.h"
#include "lib/cw-gfx-engine/Locator.h"
#include "lib/cw-gfx-engine/Game.h"
#include "lib/cw-gfx-engine/Object.h"
#include "lib/cw-gfx-engine/ImageUtils.h"
// Commons
#include "lib/cw-commons/IClockface.h"
#include "lib/cw-commons/CWDateTime.h"

#include "gfx/assets.h"
#include "gfx/mario.h"
#include "gfx/block.h"

class MarioClockface: public IClockface {
  private:
    Adafruit_GFX* _display;
    CWDateTime* _dateTime;
    void updateTime();

  public:
    MarioClockface(Adafruit_GFX* display);
    void setup(CWDateTime *dateTime);
    void update();
    void externalEvent(int type);

};
