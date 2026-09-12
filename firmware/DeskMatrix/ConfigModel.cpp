// firmware/DeskMatrix/ConfigModel.cpp
#include "ConfigModel.h"
#include <ArduinoJson.h>

namespace {
// Empty is always valid (no custom theme configured yet). Otherwise: size
// capped at 64KB (comfortably above the largest known real clock-club
// theme, ~13.5KB), must be valid JSON, and must have a 'setup' or 'loop'
// array -- catches an empty-object paste or a copy-paste of the wrong
// thing without needing a full CanvasClockface parse here.
bool isValidCanvasJson(const std::string& json, std::string& error) {
    if (json.empty()) return true;
    if (json.size() > 65536) {
        error = "canvasJson exceeds 64KB limit";
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        error = "canvasJson is not valid JSON";
        return false;
    }
    if (!doc["setup"].is<JsonArray>() && !doc["loop"].is<JsonArray>()) {
        error = "canvasJson must have a 'setup' or 'loop' array";
        return false;
    }
    return true;
}
}  // namespace

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

    std::string canvasJson = std::string(doc["canvasJson"] | "");
    std::string canvasError;
    if (!isValidCanvasJson(canvasJson, canvasError)) {
        error = canvasError;
        return false;
    }
    out.canvasJson = canvasJson;

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
    doc["canvasJson"] = config.canvasJson;
    doc["sleep"] = config.sleep;

    std::string out;
    serializeJson(doc, out);
    return out;
}
