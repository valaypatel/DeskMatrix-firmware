// firmware/DeskMatrix/screens/clockfaces/canvas/CanvasClockface.h
#pragma once
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <string>
#include <vector>

#include "lib/cw-commons/IClockface.h"
#include "lib/cw-commons/CWDateTime.h"
#include "fonts/atari.h"
#include "fonts/hour8pt7b.h"
#include "fonts/minute7pt7b.h"
#include <Fonts/Picopixel.h>

// Interprets a Clockwise "Canvas" (cw-cf-0x07) theme JSON string at
// construction time -- see docs/superpowers/specs/2026-09-12-canvas-clockface-design.md
// for the JSON schema and rationale. Draws setup[] elements once in
// setup(), then redraws loop[] sprites (advancing multi-frame animation)
// and refreshes datetime elements every update().
class CanvasClockface : public IClockface {
  public:
    // themeJson is only read during construction (not retained) -- caller
    // doesn't need to keep it alive afterward. isValid() is false if
    // themeJson failed to parse as JSON; the caller (ClockScreen.cpp) is
    // responsible for falling back to another clockface in that case.
    CanvasClockface(Adafruit_GFX* display, const char* themeJson);
    ~CanvasClockface() override;

    void setup(CWDateTime* dateTime) override;
    void update() override;

    bool isValid() const { return valid_; }

  private:
    struct SetupElement {
        enum Type { RECT, FILLRECT, LINE, TEXT, DATETIME, IMAGE } type = RECT;
        int16_t x = 0, y = 0, x1 = 0, y1 = 0, width = 0, height = 0;
        uint16_t color = 0, fgColor = 0xFFFF, bgColor = 0;
        std::string content;  // text content, or datetime format, or base64 PNG for image
        std::string font;
    };
    struct SpriteFrame {
        uint16_t* pixels = nullptr;  // owned, heap_caps_malloc'd (PSRAM); freed in destructor
        uint16_t width = 0;
        uint16_t height = 0;
    };
    struct LoopSprite {
        int spriteIndex = -1;  // index into loadedSprites_
        int16_t x = 0, y = 0;
        size_t currentFrame = 0;
    };

    void renderShape(const SetupElement& el);
    void renderText(const std::string& text, const SetupElement& el);
    void renderImageElement(const SetupElement& el);
    void renderDatetimeElements();
    void setFontByName(const std::string& name);
    bool loadSpriteFrame(const std::string& base64, SpriteFrame& outFrame);

    Adafruit_GFX* display_ = nullptr;
    CWDateTime* dateTime_ = nullptr;
    bool valid_ = false;
    uint16_t bgColor_ = 0;
    uint16_t delayMs_ = 300;
    unsigned long lastLoopMs_ = 0;
    unsigned long lastDateTimeMs_ = 0;

    std::vector<SetupElement> setupElements_;
    std::vector<std::vector<SpriteFrame>> loadedSprites_;  // [spriteIndex][frameIndex]
    std::vector<LoopSprite> loopSprites_;
};
