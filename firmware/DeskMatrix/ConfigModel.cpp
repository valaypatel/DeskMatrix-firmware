// firmware/DeskMatrix/ConfigModel.cpp
#include "ConfigModel.h"
#include <ArduinoJson.h>

bool parseConfig(const std::string& json, AppConfig& out, std::string& error) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        error = err.c_str();
        return false;
    }

    JsonObject spotify = doc["spotify"].as<JsonObject>();
    out.spotify.enabled = spotify["enabled"] | true;
    out.spotify.clientId = std::string(spotify["clientId"] | "");
    out.spotify.clientSecret = std::string(spotify["clientSecret"] | "");
    out.spotify.refreshToken = std::string(spotify["refreshToken"] | "");
    out.spotify.pollSec = spotify["pollSec"] | 5;

    out.dndArt = std::string(doc["dnd"]["art"] | "dnd_default");
    out.brbArt = std::string(doc["brb"]["art"] | "brb_default");
    out.clockFace = std::string(doc["clockFace"] | "mario");

    int brightness = doc["brightness"] | 90;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    out.brightness = brightness;

    int tzOffset = doc["timezoneOffsetMinutes"] | 0;
    if (tzOffset < -720) tzOffset = -720; // UTC-12:00
    if (tzOffset > 840) tzOffset = 840;   // UTC+14:00
    out.timezoneOffsetMinutes = tzOffset;

    out.sleep = doc["sleep"] | false;

    return true;
}

std::string serializeConfig(const AppConfig& config) {
    JsonDocument doc;

    JsonObject spotify = doc["spotify"].to<JsonObject>();
    spotify["enabled"] = config.spotify.enabled;
    spotify["clientId"] = config.spotify.clientId;
    spotify["clientSecret"] = config.spotify.clientSecret;
    spotify["refreshToken"] = config.spotify.refreshToken;
    spotify["pollSec"] = config.spotify.pollSec;

    doc["dnd"]["art"] = config.dndArt;
    doc["brb"]["art"] = config.brbArt;
    doc["clockFace"] = config.clockFace;
    doc["brightness"] = config.brightness;
    doc["timezoneOffsetMinutes"] = config.timezoneOffsetMinutes;
    doc["sleep"] = config.sleep;

    std::string out;
    serializeJson(doc, out);
    return out;
}
