// firmware/DeskMatrix/ConfigModel.h
#pragma once
#include <string>

struct SpotifyConfig {
    bool enabled = true; // when false, Spotify polling/screen are skipped even if credentials are set
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    int pollSec = 5;
};

struct AppConfig {
    SpotifyConfig spotify;
    std::string dndArt = "dnd_default";
    std::string brbArt = "brb_default";
    std::string clockFace = "mario"; // "mario" | "words" | "pacman" — idle-screen clockface preset
    int brightness = 90; // 0-255, passed directly to MatrixPanel_I2S_DMA::setBrightness8()
    int timezoneOffsetMinutes = 0; // UTC offset, e.g. 330 for IST (+05:30); passed to configTime()
    bool sleep = false; // when true, screen renders black but WiFi/config/Spotify/IMU keep running; auto-wakes on Spotify playback
    std::string canvasJson; // user-pasted Canvas clockface theme JSON (see screens/clockfaces/canvas/); empty = none configured
};

// Returns true and fills `out` on success; returns false and fills `error` on failure.
bool parseConfig(const std::string& json, AppConfig& out, std::string& error);

std::string serializeConfig(const AppConfig& config);
