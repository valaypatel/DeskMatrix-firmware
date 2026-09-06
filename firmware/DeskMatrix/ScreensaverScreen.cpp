// firmware/DeskMatrix/ScreensaverScreen.cpp
//
// NOT VERIFIED AGAINST REAL HARDWARE — AnimatedGIF's exact callback
// signatures and palette-mode constant name have varied slightly between
// library releases. Confirm this compiles as-is against whatever version
// `arduino-cli lib install AnimatedGIF` pulls; expected to be a small
// signature fix (e.g. the palette-mode constant name), not a redesign — the
// same kind of one-time hardware calibration ImuHardware.cpp already needed.
#include "screens/ScreensaverScreen.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>

namespace {
const char* kScreensaverPath = "/screensaver.gif";
AnimatedGIF g_gif;
File g_gifFile;
bool g_loaded = false;
unsigned long g_nextFrameDueMs = 0;
MatrixPanel_I2S_DMA* g_display = nullptr; // set only for the duration of one playFrame() call

void* gifOpen(const char* filename, int32_t* size) {
    g_gifFile = LittleFS.open(filename, "r");
    *size = g_gifFile.size();
    return &g_gifFile;
}
void gifClose(void*) {
    if (g_gifFile) g_gifFile.close();
}
int32_t gifRead(GIFFILE*, uint8_t* buffer, int32_t length) {
    if (!g_gifFile) return 0;
    return g_gifFile.read(buffer, length);
}
int32_t gifSeek(GIFFILE*, int32_t position) {
    if (!g_gifFile) return 0;
    g_gifFile.seek(position);
    return position;
}

// Called once per decoded scanline with pre-resolved RGB565 pixels
// (gif.begin() below selects the RGB565 draw mode rather than raw palette
// indices, so no manual palette lookup is needed here).
void gifDraw(GIFDRAW* pDraw) {
    if (!g_display) return;
    uint16_t* row = (uint16_t*)pDraw->pPixels;
    for (int x = 0; x < pDraw->iWidth; x++) {
        g_display->drawPixel(pDraw->iX + x, pDraw->iY + pDraw->y, row[x]);
    }
}
}  // namespace

bool loadScreensaverGif() {
    if (!LittleFS.exists(kScreensaverPath)) {
        g_loaded = false;
        return false;
    }
    g_gif.begin(BIG_ENDIAN_PIXELS);
    g_loaded = g_gif.open(kScreensaverPath, gifOpen, gifClose, gifRead, gifSeek, gifDraw);
    g_nextFrameDueMs = 0;
    return g_loaded;
}

void drawScreensaverFrame(MatrixPanel_I2S_DMA* display) {
    if (!display || !g_loaded) return;

    unsigned long nowMs = millis();
    if (nowMs < g_nextFrameDueMs) return;

    g_display = display;
    int delayMs = 0;
    int result = g_gif.playFrame(false, &delayMs);
    g_display = nullptr;

    if (result == 0) {
        // End of the GIF's loop. AnimatedGIF rewinds automatically for most
        // files per their own loop-count metadata, but re-open defensively
        // in case this one doesn't.
        g_gif.close();
        loadScreensaverGif();
    }

    display->flipDMABuffer();
    g_nextFrameDueMs = nowMs + (delayMs > 0 ? (unsigned long)delayMs : 100UL);
}
