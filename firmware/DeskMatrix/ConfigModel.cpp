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
    out.spotify.clientId = std::string(spotify["clientId"] | "");
    out.spotify.clientSecret = std::string(spotify["clientSecret"] | "");
    out.spotify.refreshToken = std::string(spotify["refreshToken"] | "");
    out.spotify.pollSec = spotify["pollSec"] | 5;

    out.dndArt = std::string(doc["dnd"]["art"] | "dnd_default");
    out.brbArt = std::string(doc["brb"]["art"] | "brb_default");

    int brightness = doc["brightness"] | 90;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    out.brightness = brightness;

    return true;
}

std::string serializeConfig(const AppConfig& config) {
    JsonDocument doc;

    JsonObject spotify = doc["spotify"].to<JsonObject>();
    spotify["clientId"] = config.spotify.clientId;
    spotify["clientSecret"] = config.spotify.clientSecret;
    spotify["refreshToken"] = config.spotify.refreshToken;
    spotify["pollSec"] = config.spotify.pollSec;

    doc["dnd"]["art"] = config.dndArt;
    doc["brb"]["art"] = config.brbArt;
    doc["brightness"] = config.brightness;

    std::string out;
    serializeJson(doc, out);
    return out;
}
