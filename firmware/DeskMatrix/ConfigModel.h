// firmware/DeskMatrix/ConfigModel.h
#pragma once
#include <string>

struct SpotifyConfig {
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    int pollSec = 5;
};

struct AppConfig {
    SpotifyConfig spotify;
    std::string dndArt = "dnd_default";
    std::string brbArt = "brb_default";
};

// Returns true and fills `out` on success; returns false and fills `error` on failure.
bool parseConfig(const std::string& json, AppConfig& out, std::string& error);

std::string serializeConfig(const AppConfig& config);
