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
    // HTTP Basic Auth gate for the config page and its API. Returns true if
    // the request is authorized; otherwise sends a 401 challenge itself and
    // returns false — every handler below must check this first and bail
    // out (`if (!checkAuth()) return;`) without doing any work.
    bool checkAuth();

    void handleGetRoot();
    void handleGetConfig();
    void handlePutConfig();
    void handlePostWifi();
    void handlePostAssetUpload();
    void handlePostAssetResponse();
    void handlePostScreensaverUpload();
    void handlePostScreensaverResponse();
    void handleOtaUpload();

    WebServer server_;
    AppConfig& appConfig_;
    SettingsStore& store_;
    bool configChanged_ = false;
};
