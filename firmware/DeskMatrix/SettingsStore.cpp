// firmware/DeskMatrix/SettingsStore.cpp
#include "SettingsStore.h"
#include <LittleFS.h>

static const char* kConfigPath = "/config.json";

bool SettingsStore::begin() {
    began_ = LittleFS.begin(true); // format on first-mount failure
    return began_;
}

bool SettingsStore::load(std::string& outJson) {
    if (!began_ || !LittleFS.exists(kConfigPath)) return false;
    File f = LittleFS.open(kConfigPath, "r");
    if (!f) return false;
    outJson = std::string(f.readString().c_str());
    f.close();
    return !outJson.empty();
}

bool SettingsStore::save(const std::string& json) {
    if (!began_) return false;
    File f = LittleFS.open(kConfigPath, "w");
    if (!f) return false;
    size_t written = f.print(json.c_str());
    f.close();
    return written == json.size();
}
