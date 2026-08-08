// firmware/DeskMatrix/SettingsStore.h
#pragma once
#include <string>

// Thin wrapper around the on-device filesystem for config/asset persistence.
// Must call begin() once (in setup()) before load()/save().
class SettingsStore {
public:
    bool begin();
    bool load(std::string& outJson);
    bool save(const std::string& json);

private:
    bool began_ = false;
};
