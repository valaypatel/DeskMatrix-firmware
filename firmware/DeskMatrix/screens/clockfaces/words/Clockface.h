#pragma once

#include <Arduino.h>

#include <Adafruit_GFX.h>
#include "lib/cw-gfx-engine/Tile.h"
#include "lib/cw-gfx-engine/Locator.h"
#include "lib/cw-gfx-engine/Game.h"
#include "lib/cw-gfx-engine/Object.h"
#include "lib/cw-gfx-engine/ImageUtils.h"
#include <WiFi.h>

#include "hour8pt7b.h"
#include "minute7pt7b.h"
#include "small4pt7b.h"
#include "DateI18nEN.h"

// Commons
#include "lib/cw-commons/IClockface.h"
#include "lib/cw-commons/Icons.h"


class WordsClockface: public IClockface {
  private:
    Adafruit_GFX* _display;
    CWDateTime* _dateTime;
    void timeInWords(int h, int m, char* hWords, char* mWords);
    void updateTime();
    void updateDate();
    void updateTemperature();

  public:
    WordsClockface(Adafruit_GFX* display);
    void setup(CWDateTime *dateTime);
    void update();
};
