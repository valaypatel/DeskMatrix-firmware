// firmware/DeskMatrix/RemoteClockService.cpp
#include "services/RemoteClockService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

RemoteClockService::RemoteClockService(float latitude, float longitude, unsigned long pollIntervalMs)
    : lat_(latitude), lon_(longitude), pollIntervalMs_(pollIntervalMs) {}

void RemoteClockService::loop() {
    unsigned long nowMs = millis();
    if (lastFetchMs_ != 0 && (nowMs - lastFetchMs_) < pollIntervalMs_) {
        return;
    }
    fetch();
    lastFetchMs_ = nowMs;
}

void RemoteClockService::fetch() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    char url[224];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m&timezone=auto",
        lat_, lon_);
    http.begin(url);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return;
    }

    // Buffer the full body before parsing — Open-Meteo uses chunked transfer
    // encoding, and streaming straight from http.getStream() into
    // deserializeJson() reliably fails with InvalidInput (same issue found
    // and fixed in WeatherService.cpp during MVP bring-up).
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return;

    offsetSec_ = doc["utc_offset_seconds"] | offsetSec_;
    haveOffset_ = true;
}
