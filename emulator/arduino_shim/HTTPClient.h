// emulator/arduino_shim/HTTPClient.h
#pragma once
#include "WiFiClient.h"
#include "WiFiClientSecure.h"

// Stub -- GET() always reports failure (-1), so SpotifyScreen.cpp's
// drawSpotifyScreen() always takes its "fetch failed" branch and falls
// through to renderIdleFrame(). This is the intended v1 behavior: no real
// Spotify network calls from the desktop build.
class HTTPClient {
public:
    void begin(WiFiClientSecure&, const char*) {}
    int GET() { return -1; }
    WiFiClient* getStreamPtr() { return &stream_; }
    void end() {}

private:
    WiFiClient stream_;
};
