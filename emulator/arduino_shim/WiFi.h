// emulator/arduino_shim/WiFi.h
//
// Stub -- only exists so clockfaces (e.g. WordsClockface.cpp) that check
// `WiFi.status() == WL_CONNECTED` to decide whether to show a temperature/
// network-dependent element compile and run. Always reports disconnected:
// there is no real networking in the native emulator build (see the
// WiFiClient/WiFiClientSecure/HTTPClient stubs, which are likewise
// always-fail/no-op).
#pragma once

enum wl_status_t {
    WL_NO_SHIELD = 255,
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
};

class WiFiClass {
public:
    wl_status_t status() { return WL_DISCONNECTED; }
};

inline WiFiClass WiFi;
