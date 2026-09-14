// emulator/arduino_shim/WiFiClient.h
#pragma once
#include <cstdint>
#include <cstddef>

// Stub -- only exists so HTTPClient.h's getStreamPtr() return type and
// SpotifyScreen.cpp's stream->readBytes() call compile. Never actually
// used: HTTPClient::GET() below always returns a failure status, so
// SpotifyScreen.cpp's code never reaches the point of calling readBytes().
class WiFiClient {
public:
    int readBytes(uint8_t*, size_t) { return 0; }
};
