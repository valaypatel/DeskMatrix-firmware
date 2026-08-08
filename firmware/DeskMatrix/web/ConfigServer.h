// firmware/DeskMatrix/web/ConfigServer.h
#pragma once
#include <WebServer.h>
#include "../ConfigModel.h"
#include "../SettingsStore.h"

class ConfigServer {
public:
    ConfigServer(AppConfig& appConfig, SettingsStore& store);
    void begin();
    void loop();

    // Returns true once after a successful PUT /api/config (and resets to
    // false), so DeskMatrix.ino's loop() can notice a config change and
    // re-apply it to live services (e.g. WeatherService) without a reboot.
    bool configChanged();

private:
    void handleGetConfig();
    void handlePutConfig();
    void handlePostAssetUpload();
    void handlePostAssetResponse();
    void handleOtaUpload();

    WebServer server_;
    AppConfig& appConfig_;
    SettingsStore& store_;
    bool configChanged_ = false;
};
