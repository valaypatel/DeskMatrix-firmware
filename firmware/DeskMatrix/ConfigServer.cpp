// firmware/DeskMatrix/ConfigServer.cpp
#include "web/ConfigServer.h"
#include <Update.h>
#include <LittleFS.h>

ConfigServer::ConfigServer(AppConfig& appConfig, SettingsStore& store)
    : server_(80), appConfig_(appConfig), store_(store) {}

void ConfigServer::begin() {
    server_.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
    server_.on("/api/config", HTTP_PUT, [this]() { handlePutConfig(); });
    server_.on("/api/assets", HTTP_POST, [this]() { handlePostAsset(); });
    server_.on("/api/ota", HTTP_POST,
        [this]() {
            server_.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
            delay(500);
            ESP.restart();
        },
        [this]() { handleOtaUpload(); });
    server_.begin();
}

void ConfigServer::loop() { server_.handleClient(); }

void ConfigServer::handleGetConfig() {
    server_.send(200, "application/json", serializeConfig(appConfig_).c_str());
}

void ConfigServer::handlePutConfig() {
    std::string body = server_.arg("plain").c_str();
    AppConfig parsed;
    std::string error;
    if (!parseConfig(body, parsed, error)) {
        server_.send(400, "application/json", ("{\"error\":\"" + error + "\"}").c_str());
        return;
    }
    appConfig_ = parsed;
    store_.save(serializeConfig(appConfig_));
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

void ConfigServer::handlePostAsset() {
    if (!server_.hasArg("id")) {
        server_.send(400, "text/plain", "missing ?id=");
        return;
    }
    std::string path = "/assets/" + std::string(server_.arg("id").c_str()) + ".bin";
    if (!LittleFS.exists("/assets")) LittleFS.mkdir("/assets");
    File f = LittleFS.open(path.c_str(), "w");
    if (!f) {
        server_.send(500, "text/plain", "failed to open asset file");
        return;
    }
    std::string body = server_.arg("plain").c_str();
    f.write((const uint8_t*)body.data(), body.size());
    f.close();
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

void ConfigServer::handleOtaUpload() {
    HTTPUpload& upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        Update.end(true);
    }
}
