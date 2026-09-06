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
    // Uses server_.raw() (HTTPRaw), not server_.upload() (HTTPUpload): the
    // installed WebServer version (esp32 core 3.3.11) only populates
    // _currentUpload for multipart/form-data bodies. A plain (non-multipart)
    // POST body — e.g. `curl --data-binary` — is dispatched through the raw
    // path instead, and calling upload() there dereferences a null
    // unique_ptr (crash, verified on real hardware). See ConfigServer.h.
    HTTPRaw& raw = server_.raw();
    static File assetFile;

    if (raw.status == RAW_START) {
        if (!server_.hasArg("id")) return;
        if (!LittleFS.exists("/assets")) LittleFS.mkdir("/assets");
        std::string path = "/assets/" + std::string(server_.arg("id").c_str()) + ".bin";
        assetFile = LittleFS.open(path.c_str(), "w");
    } else if (raw.status == RAW_WRITE) {
        if (assetFile) assetFile.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
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
    // See the comment in handlePostAssetUpload(): raw (non-multipart) POST
    // bodies go through server_.raw(), not server_.upload().
    HTTPRaw& raw = server_.raw();

    if (raw.status == RAW_START) {
        g_screensaverHeaderChecked = false;
        g_screensaverHeaderValid = false;
        g_screensaverTmpFile = LittleFS.open("/screensaver.gif.tmp", "w");
    } else if (raw.status == RAW_WRITE) {
        if (!g_screensaverHeaderChecked) {
            g_screensaverHeaderValid = isValidGifHeader(raw.buf, raw.currentSize);
            g_screensaverHeaderChecked = true;
        }
        if (g_screensaverTmpFile) g_screensaverTmpFile.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
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
    // See the comment in handlePostAssetUpload(): raw (non-multipart) POST
    // bodies go through server_.raw(), not server_.upload().
    HTTPRaw& raw = server_.raw();
    if (raw.status == RAW_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (raw.status == RAW_WRITE) {
        Update.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
        Update.end(true);
    }
}
