// firmware/DeskMatrix/SpotifyScreen.cpp
//
// NOT VERIFIED AGAINST REAL HARDWARE — JPEGDEC's in-memory-buffer open
// method name (openRAM here) has changed across library versions. Confirm
// against whatever version `arduino-cli lib install JPEGDEC` pulls; expected
// to be a small signature fix, not a redesign.
#include "screens/SpotifyScreen.h"
#include "services/SpotifyService.h"
#include "PanelPresent.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <vector>
#include <cmath>
#include <new>
#include "esp_heap_caps.h"

namespace {
constexpr int kSize = 64;                 // panel is fixed 64x64 (config.h)
constexpr int kMargin = 2;                // gap between record edge and panel edge
constexpr int kCenter = kSize / 2;
constexpr float kOuterRadius = kSize / 2.0f - kMargin;
constexpr float kLabelRadius = 4.0f;      // center label
constexpr float kHoleRadius = 2.0f;       // spindle hole
constexpr float kBorderWidth = 1.5f;      // record-edge ring thickness (~1-2px)
constexpr float kDegreesPerTick = 6.0f;   // spin speed: full rotation every ~6s at kTickMs
constexpr unsigned long kTickMs = 100;    // redraw cadence while on this screen

std::string g_lastDrawnUrl;
// Decoded album art, source for the spinning render. Lazily placed in
// PSRAM on first use, like the other decoder buffers -- see the
// RAM-vs-Spotify-TLS note in ScreensaverScreen.cpp.
uint16_t* albumRGB565() {
    static uint16_t* p = (uint16_t*)heap_caps_malloc(kSize * kSize * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    return p;
}
#define g_albumRGB565 albumRGB565()
bool g_albumValid = false;
float g_angleDeg = 0.0f;
unsigned long g_nextTickMs = 0;

// JPEGDEC's decode state is ~17.5KB -- see the same RAM-vs-Spotify-TLS note.
JPEGDEC& jpeg() {
    static JPEGDEC* p = new (heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_SPIRAM)) JPEGDEC();
    return *p;
}
#define g_jpeg jpeg()

int onJpegDraw(JPEGDRAW* pDraw) {
    for (int row = 0; row < pDraw->iHeight; row++) {
        int y = pDraw->y + row;
        if (y < 0 || y >= kSize) continue;
        for (int col = 0; col < pDraw->iWidth; col++) {
            int x = pDraw->x + col;
            if (x < 0 || x >= kSize) continue;
            g_albumRGB565[y * kSize + x] = pDraw->pPixels[row * pDraw->iWidth + col];
        }
    }
    return 1;
}

uint16_t colorRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Renders one frame of the spinning-record disc into `display` at the
// current g_angleDeg, sampling g_albumRGB565 with an inverse rotation
// (nearest-neighbor) so the whole disc area stays filled. Mirrors the
// visual layout of https://github.com/tnarla/spotify-matrix's render_record().
void renderRecordFrame(Adafruit_GFX* display) {
    float rad = g_angleDeg * (float)M_PI / 180.0f;
    float cosT = cosf(rad);
    float sinT = sinf(rad);
    // Source image fills the disc 1:1 at scale kSize/(2*kOuterRadius).
    float scale = (float)kSize / (2.0f * kOuterRadius);

    for (int y = 0; y < kSize; y++) {
        for (int x = 0; x < kSize; x++) {
            float dx = x - kCenter + 0.5f;
            float dy = y - kCenter + 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);

            uint16_t pixel;
            if (dist > kOuterRadius) {
                pixel = 0; // black background outside the record
            } else if (dist > kOuterRadius - kBorderWidth) {
                pixel = colorRGB565(35, 35, 35); // visible thin outline, not just near-black
            } else if (dist <= kHoleRadius) {
                pixel = 0; // spindle hole
            } else if (dist <= kLabelRadius) {
                pixel = colorRGB565(16, 16, 16); // center label
            } else {
                // Inverse-rotate this output pixel back into source space.
                float sx = (dx * cosT + dy * sinT) * scale + kCenter;
                float sy = (-dx * sinT + dy * cosT) * scale + kCenter;
                int isx = (int)(sx);
                int isy = (int)(sy);
                if (isx < 0 || isx >= kSize || isy < 0 || isy >= kSize) {
                    pixel = 0;
                } else {
                    pixel = g_albumRGB565[isy * kSize + isx];
                }
            }
            display->drawPixel(x, y, pixel);
        }
    }
    presentFrame(display);
}

void renderIdleFrame(Adafruit_GFX* display) {
    for (int y = 0; y < kSize; y++) {
        for (int x = 0; x < kSize; x++) {
            float dx = x - kCenter + 0.5f;
            float dy = y - kCenter + 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);
            uint16_t pixel;
            if (dist > kOuterRadius || dist < kOuterRadius - 1.0f) {
                pixel = (dist <= 4.0f) ? colorRGB565(18, 18, 18) : 0;
            } else {
                pixel = colorRGB565(55, 55, 55); // faint idle ring outline
            }
            display->drawPixel(x, y, pixel);
        }
    }
    presentFrame(display);
}
}  // namespace

void drawSpotifyScreen(Adafruit_GFX* display, const std::string& albumArtUrl, bool isPlaying) {
    if (!display) return;

    if (albumArtChanged(g_lastDrawnUrl, albumArtUrl)) {
        WiFiClientSecure client;
        // See SpotifyService.cpp: the vendored root cert no longer validates
        // against Spotify's current TLS chain (confirmed on real hardware).
        // Traffic stays encrypted; only server identity verification is skipped.
        client.setInsecure();
        HTTPClient http;
        http.begin(client, albumArtUrl.c_str());
        int status = http.GET();
        if (status != 200) {
            Serial.printf("[spotify] album art fetch failed, status=%d\n", status);
            http.end();
        } else {
            WiFiClient* stream = http.getStreamPtr();
            std::vector<uint8_t> jpegBytes;
            jpegBytes.reserve(8192);
            uint8_t buf[512];
            int n;
            while ((n = stream->readBytes(buf, sizeof(buf))) > 0) {
                jpegBytes.insert(jpegBytes.end(), buf, buf + n);
            }
            http.end();

            if (g_jpeg.openRAM(jpegBytes.data(), jpegBytes.size(), onJpegDraw)) {
                g_jpeg.decode(0, 0, 0);
                g_jpeg.close();
                g_lastDrawnUrl = albumArtUrl;
                g_albumValid = true;
            } else {
                Serial.println("[spotify] JPEG decode failed");
                // keep whatever was drawn before, per the design spec's error handling
            }
        }
    }

    unsigned long nowMs = millis();
    if (nowMs < g_nextTickMs) return;
    g_nextTickMs = nowMs + kTickMs;

    if (!g_albumValid) {
        renderIdleFrame(display);
        return;
    }
    if (isPlaying) {
        g_angleDeg += kDegreesPerTick;
        if (g_angleDeg >= 360.0f) g_angleDeg -= 360.0f;
    }
    renderRecordFrame(display);
}
