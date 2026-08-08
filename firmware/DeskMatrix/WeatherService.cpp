// firmware/DeskMatrix/WeatherService.cpp
#include "services/WeatherService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

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
    Serial.printf("[weather] fetch() called, lat=%.4f lon=%.4f\n", lat_, lon_);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[weather] WiFi not connected, skipping fetch");
        return;
    }

    HTTPClient http;
    char url[224];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code,is_day&timezone=auto",
        lat_, lon_);
    Serial.printf("[weather] GET %s\n", url);
    // Simplification: HTTPClient::begin(url) on esp32 core 3.x auto-creates a
    // secure client for https:// and skips certificate validation. Acceptable
    // for a public, non-sensitive weather API on an MVP; revisit if this
    // pattern gets reused for anything handling sensitive data.
    http.begin(url);
    int code = http.GET();
    Serial.printf("[weather] HTTP status: %d\n", code);
    if (code != 200) {
        Serial.println("[weather] non-200 response, keeping last known reading");
        http.end();
        return; // keep last-known-good reading, per the design spec's error handling
    }

    // Read the full body into a String before parsing rather than streaming
    // directly from http.getStream() into deserializeJson(): Open-Meteo's
    // response uses chunked transfer encoding, and ArduinoJson's stream
    // parser doesn't strip chunk-size markers, which reliably produces
    // DeserializationError::InvalidInput. Buffering first sidesteps that;
    // the response is small (well under 1KB) so this costs negligible RAM.
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[weather] JSON parse error: %s (payload: %s)\n", err.c_str(), payload.c_str());
        return;
    }

    latest_.temperatureC = doc["current"]["temperature_2m"] | latest_.temperatureC;
    latest_.weatherCode = doc["current"]["weather_code"] | latest_.weatherCode;
    latest_.isDay = (doc["current"]["is_day"] | 1) != 0;
    latest_.valid = true;
    Serial.printf("[weather] parsed OK: temp=%.1fC code=%d\n", latest_.temperatureC, latest_.weatherCode);

    // Open-Meteo's `timezone=auto` resolves the IANA zone for the requested
    // lat/lon and returns its current UTC offset directly — reuse that
    // instead of a manual/compile-time timezone setting. Re-applying this
    // on every successful fetch also picks up DST transitions automatically.
    long utcOffsetSec = doc["utc_offset_seconds"] | 0;
    Serial.printf("[weather] utc_offset_seconds=%ld, applying configTime\n", utcOffsetSec);
    configTime(utcOffsetSec, 0, "pool.ntp.org");
}
