// firmware/DeskMatrix/SpotifyScreen.cpp
//
// NOT VERIFIED AGAINST REAL HARDWARE — JPEGDEC's in-memory-buffer open
// method name (openRAM here) has changed across library versions. Confirm
// against whatever version `arduino-cli lib install JPEGDEC` pulls; expected
// to be a small signature fix, not a redesign.
#include "screens/SpotifyScreen.h"
#include "services/SpotifyService.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <vector>

// SpotifyArduinoCert.h defines spotify_server_cert/spotify_image_server_cert
// as plain (non-extern) globals with no include guard against multiple
// translation units, so it can only be #included once across the sketch —
// SpotifyService.cpp already does that. Declare the one symbol needed here
// as extern instead of re-including the header, to avoid a duplicate-symbol
// link error.
extern const char* spotify_image_server_cert;

namespace {
std::string g_lastDrawnUrl;
MatrixPanel_I2S_DMA* g_display = nullptr; // set only for the duration of one decode() call
JPEGDEC g_jpeg;

int onJpegDraw(JPEGDRAW* pDraw) {
    if (!g_display || pDraw->y >= g_display->height()) return 0;
    g_display->drawRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    return 1;
}
}  // namespace

void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl) {
    if (!display || !albumArtChanged(g_lastDrawnUrl, albumArtUrl)) return;

    WiFiClientSecure client;
    client.setCACert(spotify_image_server_cert);
    HTTPClient http;
    http.begin(client, albumArtUrl.c_str());
    int status = http.GET();
    if (status != 200) {
        Serial.printf("[spotify] album art fetch failed, status=%d\n", status);
        http.end();
        return; // keep whatever was drawn before, per the design spec's error handling
    }

    WiFiClient* stream = http.getStreamPtr();
    std::vector<uint8_t> jpegBytes;
    jpegBytes.reserve(8192);
    uint8_t buf[512];
    int n;
    while ((n = stream->readBytes(buf, sizeof(buf))) > 0) {
        jpegBytes.insert(jpegBytes.end(), buf, buf + n);
    }
    http.end();

    g_display = display;
    if (g_jpeg.openRAM(jpegBytes.data(), jpegBytes.size(), onJpegDraw)) {
        g_jpeg.decode(0, 0, 0);
        g_jpeg.close();
        g_lastDrawnUrl = albumArtUrl;
    } else {
        Serial.println("[spotify] JPEG decode failed");
        // keep whatever was drawn before, per the design spec's error handling
    }
    g_display = nullptr;
}
