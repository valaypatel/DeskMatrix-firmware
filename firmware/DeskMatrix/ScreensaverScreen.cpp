// firmware/DeskMatrix/ScreensaverScreen.cpp
#include "screens/ScreensaverScreen.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>
#include <cstring>

namespace {
constexpr int kSize = 64; // panel is fixed 64x64 (config.h)
const char* kScreensaverPath = "/screensaver.gif";
AnimatedGIF g_gif;
File g_gifFile;
bool g_loaded = false;
unsigned long g_nextFrameDueMs = 0;

// A persistent off-screen canvas, not the display's own double-buffer.
// Many real-world GIFs are delta-encoded: a frame only redraws the pixels
// that changed and relies on the rest of the previous frame staying put
// (disposal method 1, "do not dispose"). The panel's true double-buffering
// (two physical buffers ping-ponged by flipDMABuffer()) breaks that
// assumption — each frame would draw onto whatever was shown *two* frames
// ago, not one, corrupting colors on any GIF that relies on this (confirmed
// on real hardware: "doesn't keep the color"). Drawing into our own single
// persistent canvas and blitting the whole thing to the display every
// frame sidesteps the ping-pong entirely, while still fixing the earlier
// leftover-content-from-the-previous-screen bug (clear the canvas once on
// load, never mid-playback).
uint16_t g_canvas[kSize * kSize];

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
    int y = pDraw->iY + pDraw->y;
    if (y < 0 || y >= kSize) return;
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
            int dx = pDraw->iX + x;
            if (dx < 0 || dx >= kSize) continue;
            uint8_t idx = s[x];
            if (idx != transparent) {
                g_canvas[y * kSize + dx] = palette[idx];
            }
        }
    } else {
        for (int x = 0; x < pDraw->iWidth; x++) {
            int dx = pDraw->iX + x;
            if (dx < 0 || dx >= kSize) continue;
            g_canvas[y * kSize + dx] = palette[s[x]];
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
    // Clear once per load, not per frame: a GIF smaller than the panel, or
    // one that doesn't repaint every pixel every frame (delta encoding —
    // see the canvas comment above), would otherwise leave whatever the
    // previous screen (or previous GIF) drew showing through.
    memset(g_canvas, 0, sizeof(g_canvas));
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

    int delayMs = 0;
    int result = g_gif.playFrame(false, &delayMs);

    if (result == 0) {
        // End of the GIF's loop. AnimatedGIF rewinds automatically for most
        // files per their own loop-count metadata, but re-open defensively
        // in case this one doesn't.
        g_gif.close();
        loadScreensaverGif();
    }

    // Blit the whole canvas every frame (not just the pixels this frame's
    // gifDraw() touched) so the display always reflects the canvas exactly,
    // regardless of which physical buffer flipDMABuffer() is about to show.
    display->drawRGBBitmap(0, 0, g_canvas, kSize, kSize);
    display->flipDMABuffer();
    g_nextFrameDueMs = nowMs + (delayMs > 0 ? (unsigned long)delayMs : 100UL);
}
