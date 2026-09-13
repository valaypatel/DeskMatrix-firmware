// firmware/DeskMatrix/ScreensaverScreen.cpp
#include "screens/ScreensaverScreen.h"
#include "PanelPresent.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>
#include <cstring>
#include <string>
#include <new>
#include "esp_heap_caps.h"

namespace {
constexpr int kSize = 64; // panel is fixed 64x64 (config.h)
const char* kScreensaverPath = "/screensaver.gif";
const char* kDndPath = "/dnd.gif";

// One shared AnimatedGIF instance for both the screensaver and the DND
// custom-image screen, reloading whenever the requested path differs from
// what's currently loaded (mirrors SpotifyScreen's albumArtChanged()
// pattern) — they're never active at the same time (mutually exclusive
// ScreenModes), so there's no need for (and no RAM cost from) a second
// decoder instance. A second AnimatedGIF+JPEGDEC pair was tried for this
// exact purpose once before and pushed RAM usage from 33% to 48%, which
// broke Spotify's TLS handshake (see the "custom image" work this session
// that was reverted) — this shared-instance design avoids that entirely.
// AnimatedGIF's decode state is ~24KB -- as a plain global it was internal
// RAM that's needed elsewhere (see the RAM-vs-Spotify-TLS note above).
// Lazily placed in PSRAM on first use; the reference lets every existing
// `g_gif.foo()` call site stay unchanged.
AnimatedGIF& gif() {
    static AnimatedGIF* p = new (heap_caps_malloc(sizeof(AnimatedGIF), MALLOC_CAP_SPIRAM)) AnimatedGIF();
    return *p;
}
#define g_gif gif()
File g_gifFile;
bool g_loaded = false;
std::string g_loadedPath; // which path is currently loaded, empty if none
unsigned long g_nextFrameDueMs = 0;

// One-shot "the screensaver GIF just wrapped a loop" flag — see
// screensaverGifLoopCompleted() below. Only set when the *screensaver* path
// (not the DND path, which shares this same decoder) is what looped.
bool g_screensaverLoopCompleted = false;

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
uint16_t* canvas() {
    static uint16_t* p = (uint16_t*)heap_caps_malloc(kSize * kSize * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    return p;
}
#define g_canvas canvas()

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
// its own ESP32 HUB75 example) — the LITTLE_ENDIAN_PIXELS/BIG_ENDIAN_PIXELS
// argument to gif.begin() only controls the byte order of the RGB565
// entries in pDraw->pPalette, it does not switch pPixels itself to raw
// RGB565. Every pixel must be looked up through the palette.
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

namespace {
// Opens `path` into g_gif without touching g_canvas — used both by
// loadPath() (after it clears the canvas) and by the loop-restart path in
// playCurrentFrame(), which must NOT clear: the GIF's own frame 1 is its
// base/keyframe and covers the canvas on its own, so clearing there too
// just inserts one all-black flashed frame at every loop boundary
// (confirmed on real hardware: a flicker each time the GIF looped).
bool openGifFile(const char* path) {
    // Matches the display library's own bundled reference example
    // (AnimatedGIFPanel_LittleFS.ino), which uses LITTLE_ENDIAN_PIXELS —
    // BIG_ENDIAN_PIXELS (an earlier, never-actually-verified choice)
    // byte-swaps the palette's RGB565 entries, tinting colors wrong
    // (confirmed on real hardware: unexpected pink where none exists).
    g_gif.begin(LITTLE_ENDIAN_PIXELS);
    g_loaded = g_gif.open(path, gifOpen, gifClose, gifRead, gifSeek, gifDraw);
    g_nextFrameDueMs = 0;
    return g_loaded;
}

void unloadCurrent() {
    if (g_loaded) g_gif.close(); // closes g_gifFile via gifClose()
    g_loaded = false;
    g_loadedPath.clear();
}

bool loadPath(const char* path) {
    unloadCurrent();
    // Clear once per load, not per frame: a GIF smaller than the panel, or
    // one that doesn't repaint every pixel every frame (delta encoding —
    // see the canvas comment above), would otherwise leave whatever the
    // previous screen (or previous GIF) drew showing through.
    memset(g_canvas, 0, kSize * kSize * sizeof(uint16_t));
    if (!LittleFS.exists(path)) return false;
    if (!openGifFile(path)) return false;
    g_loadedPath = path;
    return true;
}

// Shared by drawScreensaverFrame() and drawDndGifFrame(): both just need
// "make sure `path` is the active GIF, then advance/blit/flip one frame if
// due" — the actual playback logic doesn't care which screen is asking.
//
// No rotation compensation: DND/BRB are triggered by physically rotating
// the whole panel 90° on its desk stand (see services/ImuHardware.h), so
// content drawn in the panel's native orientation displays sideways to the
// viewer once it's turned. Tried counter-rotating the content in code to
// compensate, but the direction proved hard to pin down reliably from
// verbal feedback alone (real hardware testing cycled through 90° CW, 90°
// CCW, and back without converging) — simpler and more reliable to upload
// DND/BRB GIFs already rotated to look correct in their target physical
// orientation, and just display them as-is.
void playCurrentFrame(Adafruit_GFX* display, const char* path) {
    if (!display) return;
    if (g_loadedPath != path) {
        loadPath(path);
    }
    if (!g_loaded) return;

    unsigned long nowMs = millis();
    if (nowMs < g_nextFrameDueMs) return;

    int delayMs = 0;
    int result = g_gif.playFrame(false, &delayMs);

    if (result == 0) {
        // End of the GIF's loop. AnimatedGIF rewinds automatically for most
        // files per their own loop-count metadata, but re-open defensively
        // in case this one doesn't — without clearing the canvas (see
        // openGifFile()'s comment).
        g_gif.close();
        openGifFile(path);
        // Pointer comparison is safe here: both call sites below always pass
        // the same string-literal pointer (kScreensaverPath/kDndPath), never
        // a copy, so identity comparison is equivalent to path comparison.
        if (path == kScreensaverPath) g_screensaverLoopCompleted = true;
    }

    // Blit the whole canvas every frame (not just the pixels this frame's
    // gifDraw() touched) so the display always reflects the canvas exactly,
    // regardless of which physical buffer flipDMABuffer() is about to show.
    display->drawRGBBitmap(0, 0, g_canvas, kSize, kSize);
    presentFrame(display);
    g_nextFrameDueMs = nowMs + (delayMs > 0 ? (unsigned long)delayMs : 100UL);
}
}  // namespace

void unloadScreensaverGif() {
    if (g_loadedPath == kScreensaverPath) unloadCurrent();
}

void unloadDndGif() {
    if (g_loadedPath == kDndPath) unloadCurrent();
}

bool loadScreensaverGif() {
    return loadPath(kScreensaverPath);
}

void drawScreensaverFrame(Adafruit_GFX* display) {
    playCurrentFrame(display, kScreensaverPath);
}

bool dndGifExists() {
    return LittleFS.exists(kDndPath);
}

void drawDndGifFrame(Adafruit_GFX* display) {
    playCurrentFrame(display, kDndPath);
}

bool screensaverGifLoopCompleted() {
    bool result = g_screensaverLoopCompleted;
    g_screensaverLoopCompleted = false;
    return result;
}
