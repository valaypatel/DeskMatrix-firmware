// firmware/DeskMatrix/WeatherService.cpp
#include "services/WeatherService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherService::WeatherService(float latitude, float longitude, int pollSec)
    : lat_(latitude), lon_(longitude), pollSec_(pollSec) {}

void WeatherService::configure(float latitude, float longitude, int pollSec) {
    lat_ = latitude;
    lon_ = longitude;
    pollSec_ = pollSec;
    lastFetchMs_ = 0; // force an immediate fetch on the next loop()
}

void WeatherService::loop() {
    unsigned long nowMs = millis();
    if (lastFetchMs_ != 0 && (nowMs - lastFetchMs_) < (unsigned long)pollSec_ * 1000UL) {
        return;
    }
    fetch();
    lastFetchMs_ = nowMs;
}

void WeatherService::fetch() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    char url[192];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code",
        lat_, lon_);
    // Simplification: HTTPClient::begin(url) on esp32 core 3.x auto-creates a
    // secure client for https:// and skips certificate validation. Acceptable
    // for a public, non-sensitive weather API on an MVP; revisit if this
    // pattern gets reused for anything handling sensitive data.
    http.begin(url);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return; // keep last-known-good reading, per the design spec's error handling
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return;

    latest_.temperatureC = doc["current"]["temperature_2m"] | latest_.temperatureC;
    latest_.weatherCode = doc["current"]["weather_code"] | latest_.weatherCode;
    latest_.valid = true;
}
