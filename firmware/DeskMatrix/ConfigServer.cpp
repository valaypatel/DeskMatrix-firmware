// firmware/DeskMatrix/ConfigServer.cpp
#include "web/ConfigServer.h"
#include "GifValidation.h"
#include <Update.h>
#include <LittleFS.h>

ConfigServer::ConfigServer(AppConfig& appConfig, SettingsStore& store)
    : server_(80), appConfig_(appConfig), store_(store) {}

void ConfigServer::begin() {
    server_.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
    server_.on("/api/config", HTTP_PUT, [this]() { handlePutConfig(); });
    server_.on("/api/assets", HTTP_POST,
        [this]() { handlePostAssetResponse(); },
        [this]() { handlePostAssetUpload(); });
    server_.on("/api/screensaver", HTTP_POST,
        [this]() { handlePostScreensaverResponse(); },
        [this]() { handlePostScreensaverUpload(); });
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

bool ConfigServer::configChanged() {
    bool changed = configChanged_;
    configChanged_ = false;
    return changed;
}

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
    configChanged_ = true;
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

void ConfigServer::handlePostAssetUpload() {
    HTTPUpload& upload = server_.upload();
    static File assetFile;

    if (upload.status == UPLOAD_FILE_START) {
        if (!server_.hasArg("id")) return;
        if (!LittleFS.exists("/assets")) LittleFS.mkdir("/assets");
        std::string path = "/assets/" + std::string(server_.arg("id").c_str()) + ".bin";
        assetFile = LittleFS.open(path.c_str(), "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (assetFile) assetFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (assetFile) assetFile.close();
    }
}

void ConfigServer::handlePostAssetResponse() {
    if (!server_.hasArg("id")) {
        server_.send(400, "text/plain", "missing ?id=");
        return;
    }
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

namespace {
File g_screensaverTmpFile;
bool g_screensaverHeaderChecked = false;
bool g_screensaverHeaderValid = false;
}  // namespace

void ConfigServer::handlePostScreensaverUpload() {
    HTTPUpload& upload = server_.upload();

    if (upload.status == UPLOAD_FILE_START) {
        g_screensaverHeaderChecked = false;
        g_screensaverHeaderValid = false;
        g_screensaverTmpFile = LittleFS.open("/screensaver.gif.tmp", "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!g_screensaverHeaderChecked) {
            g_screensaverHeaderValid = isValidGifHeader(upload.buf, upload.currentSize);
            g_screensaverHeaderChecked = true;
        }
        if (g_screensaverTmpFile) g_screensaverTmpFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (g_screensaverTmpFile) g_screensaverTmpFile.close();
        if (g_screensaverHeaderValid) {
            if (LittleFS.exists("/screensaver.gif")) LittleFS.remove("/screensaver.gif");
            LittleFS.rename("/screensaver.gif.tmp", "/screensaver.gif");
        } else {
            LittleFS.remove("/screensaver.gif.tmp"); // reject: previous screensaver (if any) stays active
        }
    }
}

void ConfigServer::handlePostScreensaverResponse() {
    if (!g_screensaverHeaderValid) {
        server_.send(400, "application/json", "{\"error\":\"not a valid GIF file\"}");
        return;
    }
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
