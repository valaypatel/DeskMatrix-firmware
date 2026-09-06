// firmware/DeskMatrix/ScreensaverScreen.cpp
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
// AnimatedGIF never advances GIFFILE::iPos itself — the read/seek callbacks
// own that bookkeeping (see the library's own ESP32 HUB75 example). Leaving
// it at 0 forever (as an earlier version of this file did) breaks the
// library's EOF/rewind detection and its playFrame() "more frames left"
// return value, since both are computed from iPos.
int32_t gifRead(GIFFILE* pFile, uint8_t* buffer, int32_t length) {
    if (!g_gifFile) return 0;
    int32_t bytesRead = length;
    if ((pFile->iSize - pFile->iPos) < length) {
        bytesRead = pFile->iSize - pFile->iPos - 1; // avoid reading the literal last byte (upstream quirk)
    }
    if (bytesRead <= 0) return 0;
    bytesRead = g_gifFile.read(buffer, bytesRead);
    pFile->iPos = g_gifFile.position();
    return bytesRead;
}
int32_t gifSeek(GIFFILE* pFile, int32_t position) {
    if (!g_gifFile) return 0;
    g_gifFile.seek(position);
    pFile->iPos = g_gifFile.position();
    return pFile->iPos;
}

// Called once per decoded scanline. pDraw->pPixels is always an 8-bit
// palette-index array in this library (confirmed against AnimatedGIF.h and
// its own ESP32 HUB75 example) — BIG_ENDIAN_PIXELS only controls the byte
// order of the RGB565 entries in pDraw->pPalette, it does not switch
// pPixels itself to raw RGB565. Every pixel must be looked up through the
// palette.
void gifDraw(GIFDRAW* pDraw) {
    if (!g_display) return;
    int y = pDraw->iY + pDraw->y;
    uint8_t* s = pDraw->pPixels;
    uint16_t* palette = pDraw->pPalette;

    if (pDraw->ucDisposalMethod == 2) { // restore to background color
        for (int x = 0; x < pDraw->iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
        }
        pDraw->ucHasTransparency = 0;
    }

    if (pDraw->ucHasTransparency) {
        uint8_t transparent = pDraw->ucTransparent;
        for (int x = 0; x < pDraw->iWidth; x++) {
            uint8_t idx = s[x];
            if (idx != transparent) {
                g_display->drawPixel(pDraw->iX + x, y, palette[idx]);
            }
        }
    } else {
        for (int x = 0; x < pDraw->iWidth; x++) {
            g_display->drawPixel(pDraw->iX + x, y, palette[s[x]]);
        }
    }
}
}  // namespace

void unloadScreensaverGif() {
    if (g_loaded) g_gif.close(); // closes g_gifFile via gifClose()
    g_loaded = false;
}

bool loadScreensaverGif() {
    unloadScreensaverGif(); // release any previously-open file first
    if (!LittleFS.exists(kScreensaverPath)) {
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

    // Clear before drawing: a GIF smaller than the 64x64 panel, or one that
    // doesn't cover every pixel every frame, would otherwise leave whatever
    // was drawn by the previous screen (e.g. the Spotify record) showing
    // through in the uncovered area — confirmed on real hardware.
    display->clearScreen();

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
