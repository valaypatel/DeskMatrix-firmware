// emulator/arduino_shim/HTTPClient.h
#pragma once
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
#include "Arduino.h"

// Stub -- GET() always reports failure (-1), so SpotifyScreen.cpp's
// drawSpotifyScreen() always takes its "fetch failed" branch and falls
// through to renderIdleFrame(). This is the intended v1 behavior: no real
// Spotify network calls from the desktop build.
//
// Because this stub returns instantly (unlike a real blocking network GET),
// g_lastDrawnUrl in SpotifyScreen.cpp never gets updated, so its
// albumArtChanged() check is true on every call -- meaning GET() itself
// would otherwise be invoked, and would log, on every single frame (~60/s)
// while the Spotify scene is active. lastLogMs throttles the failure log
// to at most once per second so the emulator's console stays readable;
// GET() itself still always returns -1 on every call.
class HTTPClient {
public:
    void begin(WiFiClientSecure&, const char*) {}
    int GET() {
        unsigned long now = millis();
        if (now - lastLogMs_ >= 1000) {
            lastLogMs_ = now;
            Serial.printf("[HTTPClient stub] GET() always fails in the emulator (throttled log, once/sec)\n");
        }
        return -1;
    }
    WiFiClient* getStreamPtr() { return &stream_; }
    void end() {}

private:
    WiFiClient stream_;
    unsigned long lastLogMs_ = 0;
};
