// emulator/arduino_shim/WiFiClientSecure.h
#pragma once

// Stub -- satisfies SpotifyScreen.cpp's `WiFiClientSecure client;
// client.setInsecure();` call. No real TLS/networking on the native
// target for v1 (see docs/superpowers/specs/2026-09-13-desktop-emulator-design.md).
class WiFiClientSecure {
public:
    void setInsecure() {}
};
