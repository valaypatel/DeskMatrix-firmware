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

private:
    void handleGetConfig();
    void handlePutConfig();
    void handlePostAsset();
    void handleOtaUpload();

    WebServer server_;
    AppConfig& appConfig_;
    SettingsStore& store_;
};
