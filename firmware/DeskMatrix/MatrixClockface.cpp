// firmware/DeskMatrix/MatrixClockface.cpp
// Lives at the sketch root (not screens/), like MarioClockface.cpp/
// WordsClockface.cpp/PacmanClockface.cpp -- arduino-cli only
// auto-compiles .cpp files found directly in the sketch root.
#include "screens/clockfaces/matrix/Clockface.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace {
constexpr int kCharW = 6;  // Adafruit_GFX default font glyph advance at textSize 1
constexpr int kCharH = 8;
constexpr int kCols = 64 / kCharW;  // 10
constexpr int kRows = 64 / kCharH;  // 8

// Adafruit_GFX's built-in font only covers ASCII 32-126 -- no katakana/
// Unicode glyphs -- so the "code" is printable ASCII rather than the
// movie's actual half-width katakana, same tradeoff most software Matrix-
// rain effects outside the film make.
constexpr char kMatrixChars[] = "01023456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$%#@&*+-<>/\\|";
constexpr int kMatrixCharsLen = sizeof(kMatrixChars) - 1;  // exclude the trailing '\0'

uint16_t colorRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

char randomGlyph() {
    return kMatrixChars[rand() % kMatrixCharsLen];
}

struct Column {
    int headRow;            // can go negative (staggers entry) or past kRows (pause before respawn)
    int speed;               // ticks per row-advance; lower is faster
    int tickCounter;
    char glyphs[kRows];
    uint8_t bright[kRows];   // 0 = off, 255 = brightest (freshly lit head)
};

Column g_columns[kCols];
bool g_seeded = false;

void resetColumn(Column& col) {
    col.headRow = -(rand() % kRows);  // stagger restarts so columns don't all relaunch in sync
    col.speed = 2 + (rand() % 4);     // 2-5 ticks per step: varied fall speed per column
    col.tickCounter = 0;
}
}  // namespace

MatrixClockface::MatrixClockface(Adafruit_GFX* display) : display_(display), dateTime_(nullptr) {}

void MatrixClockface::setup(CWDateTime* dateTime) {
    dateTime_ = dateTime;
    if (!g_seeded) {
        // millis() at first setup() is a fine-enough seed -- this is a
        // decorative rain effect, not anything requiring real randomness.
        srand(static_cast<unsigned>(millis()));
        g_seeded = true;
    }
    display_->setTextWrap(false);
    for (int c = 0; c < kCols; c++) {
        std::memset(g_columns[c].bright, 0, sizeof(g_columns[c].bright));
        for (int r = 0; r < kRows; r++) g_columns[c].glyphs[r] = randomGlyph();
        resetColumn(g_columns[c]);
    }
}

void MatrixClockface::update() {
    // Full redraw every frame, not incremental -- matches ClockScreen.cpp's
    // g_canvas double-buffer blit, which needs every frame complete
    // regardless of which physical buffer flipDMABuffer() is about to show
    // (see ClockScreen.cpp's g_canvas comment for the flashing bug this
    // avoids).
    display_->fillScreen(0);

    for (int c = 0; c < kCols; c++) {
        Column& col = g_columns[c];
        for (int r = 0; r < kRows; r++) {
            if (col.bright[r] > 25) col.bright[r] -= 25; else col.bright[r] = 0;
        }
        col.tickCounter++;
        if (col.tickCounter >= col.speed) {
            col.tickCounter = 0;
            col.headRow++;
            if (col.headRow >= 0 && col.headRow < kRows) {
                col.glyphs[col.headRow] = randomGlyph();
                col.bright[col.headRow] = 255;
            }
            // Give the column's trail room to fully fade before respawning
            // it above the screen, rather than restarting the instant the
            // head exits the bottom.
            if (col.headRow >= kRows * 2) {
                resetColumn(col);
            }
        }
    }

    display_->setTextSize(1);
    for (int c = 0; c < kCols; c++) {
        for (int r = 0; r < kRows; r++) {
            uint8_t b = g_columns[c].bright[r];
            if (b == 0) continue;
            uint16_t color = (b > 200) ? colorRGB565(200, 255, 200)  // bright near-white head
                                        : colorRGB565(0, b, 0);       // green trail, scaled by brightness
            display_->setCursor(c * kCharW, r * kCharH);
            display_->setTextColor(color);
            display_->print(g_columns[c].glyphs[r]);
        }
    }

    // HUD overlay: boxed time, centered.
    char timeBuf[6];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", dateTime_->getHour(), dateTime_->getMinute());
    display_->setTextSize(2);
    int16_t x1, y1;
    uint16_t textW, textH;
    display_->getTextBounds(timeBuf, 0, 0, &x1, &y1, &textW, &textH);
    const int boxPad = 3;
    const int boxX = (64 - static_cast<int>(textW)) / 2 - boxPad;
    const int boxY = (64 - static_cast<int>(textH)) / 2 - boxPad;
    const int boxW = static_cast<int>(textW) + boxPad * 2;
    const int boxH = static_cast<int>(textH) + boxPad * 2;
    display_->fillRect(boxX, boxY, boxW, boxH, colorRGB565(0, 0, 0));
    display_->drawRect(boxX, boxY, boxW, boxH, colorRGB565(0, 180, 0));
    display_->setTextColor(colorRGB565(180, 255, 180));
    display_->setCursor((64 - static_cast<int>(textW)) / 2 - x1, (64 - static_cast<int>(textH)) / 2 - y1);
    display_->print(timeBuf);
    display_->setTextSize(1);
}
