# Office Desk Display (Screensaver + Spotify) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the home clock/weather dashboard with an office-desk build: a looping GIF screensaver as the default state, and full-screen Spotify album art whenever music is actively playing — per `docs/superpowers/specs/2026-09-06-office-desk-display-design.md`.

**Architecture:** Pure logic (config schema, state machine transitions, GIF-header validation, album-art change detection) is portable C++ with no Arduino dependency, TDD'd natively via the existing `tests/native` harness (plain g++, no framework). Hardware-dependent pieces (JPEG/GIF decode-to-panel, Spotify's HTTP polling, the screensaver upload endpoint) build on top and are verified on the real ESP32-S3-RGB-Matrix board, since there is no meaningful way to test panel rendering or live network calls off-hardware — this mirrors how `WeatherService`/`ImuHardware` were handled in the original platform build. The one-time Spotify OAuth bootstrap is a small standalone Python script, TDD'd with `unittest`, that never runs on the device itself.

**Tech Stack:** Arduino framework via `arduino-cli`, `esp32:esp32` core, `ESP32-HUB75-MatrixPanel-I2S-DMA`, `ArduinoJson`, built-in `WebServer`/`Update`/`LittleFS`. New libraries: `SpotifyArduino` (witnessmenow/spotify-api-arduino — install from its GitHub zip via Arduino IDE/`arduino-cli lib install --zip-path`), `JPEGDEC` (bitbank2, Library Manager: "JPEGDEC"), `AnimatedGIF` (bitbank2, Library Manager: "AnimatedGIF"). Python 3 stdlib only for the bootstrap script (no pip dependencies).

## Global Constraints

- Board: Waveshare ESP32-S3-RGB-Matrix (ESP32-S3-N32R16), 64×64 HUB75 panel
- Compile/upload FQBN: `esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc`, run from inside `firmware/DeskMatrix/`
- Native (host) tests: `bash tests/native/run_tests.sh` from the repo root, `g++ -std=c++17`, no external test framework
- Only one screensaver GIF is ever active; `POST /api/screensaver` always overwrites it, no `id` parameter
- Spotify credentials (`clientId`/`clientSecret`/`refreshToken`) live only in device config (`PUT /api/config`, persisted to LittleFS) — never hardcoded in firmware source
- Spotify polling default: every 5 seconds, configurable via `spotify.pollSec`
- Album art is only re-fetched/re-decoded when its URL actually changes — never re-decode on every render tick
- No track/artist text overlay in this phase — album art only
- Malformed `PUT /api/config` or `POST /api/screensaver`: rejected with 400, previous valid state stays active, device never bricks
- `DND`/`BRB` tilt modes are unchanged and still override every other mode
- Work happens in the git worktree `.worktrees/led-matrix-platform-mvp` on branch `led-matrix-platform-mvp`; all firmware paths below are relative to that worktree's root

---

## Task 1: Remove the clock/weather Home dashboard

**Files:**
- Delete: `firmware/DeskMatrix/screens/ClockWidget.h`
- Delete: `firmware/DeskMatrix/ClockWidget.cpp`
- Delete: `firmware/DeskMatrix/screens/WeatherWidget.h`
- Delete: `firmware/DeskMatrix/WeatherWidget.cpp`
- Delete: `firmware/DeskMatrix/services/WeatherService.h`
- Delete: `firmware/DeskMatrix/WeatherService.cpp`
- Delete: `firmware/DeskMatrix/services/RemoteClockService.h`
- Delete: `firmware/DeskMatrix/RemoteClockService.cpp`
- Delete: `firmware/DeskMatrix/services/WeatherIconMap.h`
- Delete: `firmware/DeskMatrix/screens/IconWidget.h`
- Delete: `firmware/DeskMatrix/IconWidget.cpp`
- Delete: `firmware/DeskMatrix/WidgetRegistry.h`
- Delete: `tests/native/test_weather_icon_map.cpp`
- Delete: `tests/native/test_remote_clock_time.cpp`
- Delete: `tests/native/test_widget_registry.cpp`

**Interfaces:**
- Produces: nothing — this task only removes code. Later tasks (2+) replace what these files provided.

This device no longer shows a multi-widget dashboard — every mode is a single full-screen draw (screensaver, Spotify art, DND, BRB), so the widget-dispatch registry these files supported no longer has a reason to exist. Removing them now (rather than leaving them unregistered) keeps the codebase honest about what's actually running.

- [ ] **Step 1: Delete the files**

```bash
git rm firmware/DeskMatrix/screens/ClockWidget.h \
       firmware/DeskMatrix/ClockWidget.cpp \
       firmware/DeskMatrix/screens/WeatherWidget.h \
       firmware/DeskMatrix/WeatherWidget.cpp \
       firmware/DeskMatrix/services/WeatherService.h \
       firmware/DeskMatrix/WeatherService.cpp \
       firmware/DeskMatrix/services/RemoteClockService.h \
       firmware/DeskMatrix/RemoteClockService.cpp \
       firmware/DeskMatrix/services/WeatherIconMap.h \
       firmware/DeskMatrix/screens/IconWidget.h \
       firmware/DeskMatrix/IconWidget.cpp \
       firmware/DeskMatrix/WidgetRegistry.h \
       tests/native/test_weather_icon_map.cpp \
       tests/native/test_remote_clock_time.cpp \
       tests/native/test_widget_registry.cpp
```

- [ ] **Step 2: Run the native test suite to confirm nothing else references the removed files yet**

Run: `bash tests/native/run_tests.sh` (from repo root)
Expected: FAIL — `test_config_model.cpp` still references the old `home`/`dataSources` schema (fixed in Task 2), and `DeskMatrix.ino` still `#include`s the deleted headers (fixed in Task 9a). This step is just confirming the deletion itself didn't silently succeed against stale caches; the failures you see here should only be about the old schema/includes, not missing-file errors for anything still in the tree.

- [ ] **Step 3: Commit**

```bash
git commit -m "remove clock/weather Home dashboard (superseded by screensaver + Spotify)"
```

---

## Task 2: Config schema — `spotify` block replaces `home`/`dataSources`

**Files:**
- Modify: `firmware/DeskMatrix/ConfigModel.h`
- Modify: `firmware/DeskMatrix/ConfigModel.cpp`
- Modify: `tests/native/test_config_model.cpp`

**Interfaces:**
- Produces: `struct SpotifyConfig { std::string clientId, clientSecret, refreshToken; int pollSec = 5; }`; `struct AppConfig { SpotifyConfig spotify; std::string dndArt = "dnd_default"; std::string brbArt = "brb_default"; }`; `bool parseConfig(const std::string&, AppConfig&, std::string&)`; `std::string serializeConfig(const AppConfig&)` — same names/signatures as before, new shape
- Consumed by: Task 9a (`DeskMatrix.ino`'s `defaultConfig()`/`loadOrInitConfig()`), Task 6 (`SpotifyService` is constructed from `AppConfig::spotify`)

- [ ] **Step 1: Rewrite the test for the new schema**

```cpp
// tests/native/test_config_model.cpp
#include "test_framework.h"
#include "ConfigModel.h"

int main() {
    std::string validJson = R"({
        "spotify": {
            "clientId": "abc123",
            "clientSecret": "shh",
            "refreshToken": "AQD...",
            "pollSec": 5
        },
        "dnd": { "art": "dnd_default" },
        "brb": { "art": "brb_default" }
    })";

    AppConfig cfg;
    std::string error;
    CHECK(parseConfig(validJson, cfg, error));
    CHECK_EQ(cfg.spotify.clientId, "abc123");
    CHECK_EQ(cfg.spotify.clientSecret, "shh");
    CHECK_EQ(cfg.spotify.refreshToken, "AQD...");
    CHECK_EQ(cfg.spotify.pollSec, 5);
    CHECK_EQ(cfg.dndArt, "dnd_default");
    CHECK_EQ(cfg.brbArt, "brb_default");

    // No spotify block yet (fresh device): defaults apply, no error
    AppConfig fresh;
    std::string freshError;
    CHECK(parseConfig(R"({"dnd":{"art":"dnd_default"},"brb":{"art":"brb_default"}})", fresh, freshError));
    CHECK_EQ(fresh.spotify.clientId, "");
    CHECK_EQ(fresh.spotify.pollSec, 5);

    // Malformed JSON rejected
    AppConfig bad;
    std::string badError;
    CHECK(!parseConfig("{not json", bad, badError));
    CHECK(!badError.empty());

    // Round-trip: parse -> serialize -> parse gives back the same values
    std::string serialized = serializeConfig(cfg);
    AppConfig reparsed;
    std::string reparseError;
    CHECK(parseConfig(serialized, reparsed, reparseError));
    CHECK_EQ(reparsed.spotify.clientId, cfg.spotify.clientId);
    CHECK_EQ(reparsed.spotify.refreshToken, cfg.spotify.refreshToken);
    CHECK_EQ(reparsed.spotify.pollSec, cfg.spotify.pollSec);
    CHECK_EQ(reparsed.dndArt, cfg.dndArt);

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL to compile — `AppConfig` has no member `spotify` yet.

- [ ] **Step 3: Update `ConfigModel.h`**

```cpp
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
```

- [ ] **Step 4: Update `ConfigModel.cpp`**

```cpp
// firmware/DeskMatrix/ConfigModel.cpp
#include "ConfigModel.h"
#include <ArduinoJson.h>

bool parseConfig(const std::string& json, AppConfig& out, std::string& error) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        error = err.c_str();
        return false;
    }

    JsonObject spotify = doc["spotify"].as<JsonObject>();
    out.spotify.clientId = std::string(spotify["clientId"] | "");
    out.spotify.clientSecret = std::string(spotify["clientSecret"] | "");
    out.spotify.refreshToken = std::string(spotify["refreshToken"] | "");
    out.spotify.pollSec = spotify["pollSec"] | 5;

    out.dndArt = std::string(doc["dnd"]["art"] | "dnd_default");
    out.brbArt = std::string(doc["brb"]["art"] | "brb_default");

    return true;
}

std::string serializeConfig(const AppConfig& config) {
    JsonDocument doc;

    JsonObject spotify = doc["spotify"].to<JsonObject>();
    spotify["clientId"] = config.spotify.clientId;
    spotify["clientSecret"] = config.spotify.clientSecret;
    spotify["refreshToken"] = config.spotify.refreshToken;
    spotify["pollSec"] = config.spotify.pollSec;

    doc["dnd"]["art"] = config.dndArt;
    doc["brb"]["art"] = config.brbArt;

    std::string out;
    serializeJson(doc, out);
    return out;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `test_config_model` shows all checks passed. Other test binaries may still fail (widget registry references were removed with their test in Task 1; state-machine schema changes land in Task 3) — only `test_config_model` needs to be green after this step.

- [ ] **Step 6: Commit**

```bash
git add firmware/DeskMatrix/ConfigModel.h firmware/DeskMatrix/ConfigModel.cpp tests/native/test_config_model.cpp
git commit -m "replace home/dataSources config schema with spotify block"
```

---

## Task 3: State machine — `SCREENSAVER` replaces `HOME`, add `SPOTIFY_PLAYING`

**Files:**
- Modify: `firmware/DeskMatrix/ScreenStateMachine.h`
- Modify: `tests/native/test_state_machine.cpp`

**Interfaces:**
- Produces: `enum class ScreenMode { WIFI_SETUP, SCREENSAVER, SPOTIFY_PLAYING, INTERRUPT_TAKEOVER, DND, BRB }`; `ScreenStateMachine::spotifyStarted()`, `ScreenStateMachine::spotifyStopped()` (new); `mode()`, `wifiConfigured()`, `enterTakeover()`, `exitTakeover()`, `tiltLeft()`, `tiltRight()`, `tiltCenter()` (same names, `HOME` renamed to `SCREENSAVER` throughout)
- Consumed by: Task 9a (`DeskMatrix.ino`'s `loop()` switches rendering on `stateMachine.mode()`), Task 9b (`spotifyStarted()`/`spotifyStopped()` called based on `SpotifyService::latest().isPlaying`)

- [ ] **Step 1: Update the test for the renamed mode and new transitions**

```cpp
#include "test_framework.h"
#include "ScreenStateMachine.h"

int main() {
    ScreenStateMachine sm;
    CHECK(sm.mode() == ScreenMode::WIFI_SETUP);

    sm.wifiConfigured();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Spotify starting/stopping toggles SCREENSAVER <-> SPOTIFY_PLAYING
    sm.spotifyStarted();
    CHECK(sm.mode() == ScreenMode::SPOTIFY_PLAYING);
    sm.spotifyStopped();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Guard: spotifyStarted() has no effect outside SCREENSAVER
    sm.enterTakeover();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);
    sm.spotifyStarted();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER); // unchanged
    sm.exitTakeover();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Guard: spotifyStopped() has no effect outside SPOTIFY_PLAYING
    sm.spotifyStopped();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER); // unchanged, already wasn't playing

    // Tilt overrides SCREENSAVER, and returns to SCREENSAVER on center
    sm.tiltLeft();
    CHECK(sm.mode() == ScreenMode::DND);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Tilt overrides SPOTIFY_PLAYING, and restores it (not SCREENSAVER) on center
    sm.spotifyStarted();
    CHECK(sm.mode() == ScreenMode::SPOTIFY_PLAYING);
    sm.tiltRight();
    CHECK(sm.mode() == ScreenMode::BRB);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::SPOTIFY_PLAYING);

    // Guard: enterTakeover() has no effect outside SCREENSAVER
    ScreenStateMachine sm2;
    sm2.enterTakeover();
    CHECK(sm2.mode() == ScreenMode::WIFI_SETUP);

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL to compile — `ScreenMode::SCREENSAVER` and `spotifyStarted()`/`spotifyStopped()` don't exist yet.

- [ ] **Step 3: Update `ScreenStateMachine.h`**

```cpp
#pragma once

enum class ScreenMode {
    WIFI_SETUP,
    SCREENSAVER,
    SPOTIFY_PLAYING,
    INTERRUPT_TAKEOVER,
    DND,
    BRB
};

class ScreenStateMachine {
public:
    ScreenStateMachine() : mode_(ScreenMode::WIFI_SETUP), preTiltMode_(ScreenMode::SCREENSAVER) {}

    ScreenMode mode() const { return mode_; }

    void wifiConfigured() {
        if (mode_ == ScreenMode::WIFI_SETUP) mode_ = ScreenMode::SCREENSAVER;
    }

    // Entered when Spotify polling reports active playback; only takes effect
    // from SCREENSAVER so DND/BRB/INTERRUPT_TAKEOVER keep their priority.
    void spotifyStarted() {
        if (mode_ == ScreenMode::SCREENSAVER) mode_ = ScreenMode::SPOTIFY_PLAYING;
    }

    // Entered when Spotify polling reports playback stopped/paused.
    void spotifyStopped() {
        if (mode_ == ScreenMode::SPOTIFY_PLAYING) mode_ = ScreenMode::SCREENSAVER;
    }

    void enterTakeover() {
        if (mode_ == ScreenMode::SCREENSAVER) mode_ = ScreenMode::INTERRUPT_TAKEOVER;
    }

    void exitTakeover() {
        if (mode_ == ScreenMode::INTERRUPT_TAKEOVER) mode_ = ScreenMode::SCREENSAVER;
    }

    void tiltLeft() {
        if (mode_ != ScreenMode::DND && mode_ != ScreenMode::BRB) preTiltMode_ = mode_;
        mode_ = ScreenMode::DND;
    }

    void tiltRight() {
        if (mode_ != ScreenMode::DND && mode_ != ScreenMode::BRB) preTiltMode_ = mode_;
        mode_ = ScreenMode::BRB;
    }

    void tiltCenter() {
        if (mode_ == ScreenMode::DND || mode_ == ScreenMode::BRB) mode_ = preTiltMode_;
    }

private:
    ScreenMode mode_;
    ScreenMode preTiltMode_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `test_state_machine` shows all checks passed.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/ScreenStateMachine.h tests/native/test_state_machine.cpp
git commit -m "rename HOME to SCREENSAVER, add SPOTIFY_PLAYING mode"
```

---

## Task 4: GIF header validation helper

**Files:**
- Create: `firmware/DeskMatrix/GifValidation.h`
- Create: `tests/native/test_gif_validation.cpp`

**Interfaces:**
- Produces: `inline bool isValidGifHeader(const uint8_t* data, size_t len)`
- Consumed by: Task 5 (`ConfigServer`'s `POST /api/screensaver` handler rejects non-GIF uploads with 400)

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_gif_validation.cpp
#include "test_framework.h"
#include "GifValidation.h"

int main() {
    const uint8_t gif87a[] = {'G','I','F','8','7','a', 0x00};
    CHECK(isValidGifHeader(gif87a, sizeof(gif87a)));

    const uint8_t gif89a[] = {'G','I','F','8','9','a', 0x00};
    CHECK(isValidGifHeader(gif89a, sizeof(gif89a)));

    const uint8_t notAGif[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10}; // JPEG magic
    CHECK(!isValidGifHeader(notAGif, sizeof(notAGif)));

    const uint8_t tooShort[] = {'G','I','F'};
    CHECK(!isValidGifHeader(tooShort, sizeof(tooShort)));

    CHECK(!isValidGifHeader(nullptr, 0));

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL to compile — `GifValidation.h` doesn't exist yet.

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/GifValidation.h
#pragma once
#include <cstdint>
#include <cstddef>

// True if `data` begins with a valid GIF file signature ("GIF87a" or
// "GIF89a") — the minimum check needed to reject non-GIF uploads before
// they're written to flash as the screensaver.
inline bool isValidGifHeader(const uint8_t* data, size_t len) {
    if (!data || len < 6) return false;
    bool is87a = data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
                 data[3] == '8' && data[4] == '7' && data[5] == 'a';
    bool is89a = data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
                 data[3] == '8' && data[4] == '9' && data[5] == 'a';
    return is87a || is89a;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `test_gif_validation` shows all checks passed.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/GifValidation.h tests/native/test_gif_validation.cpp
git commit -m "add GIF header validation helper"
```

---

## Task 5: `POST /api/screensaver` upload endpoint

**Files:**
- Modify: `firmware/DeskMatrix/web/ConfigServer.h`
- Modify: `firmware/DeskMatrix/ConfigServer.cpp`

**Interfaces:**
- Consumes: `isValidGifHeader()` from Task 4
- Produces: `POST /api/screensaver` HTTP route, writes to `/screensaver.gif` on LittleFS
- Consumed by: Task 7 (`ScreensaverScreen::loadScreensaverGif()` reads `/screensaver.gif`)

No native test here — this handler is exercised through the real `WebServer`/`LittleFS` stack, which only runs on-device (same as the existing `handlePostAssetUpload`, also untested natively). Verification is manual, via `curl`, once this is flashed as part of Task 9a's integration pass.

- [ ] **Step 1: Add the route declaration to `ConfigServer.h`**

```cpp
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
    // re-apply it to live services (e.g. SpotifyService) without a reboot.
    bool configChanged();

private:
    void handleGetConfig();
    void handlePutConfig();
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
```

- [ ] **Step 2: Register the route and implement the handlers in `ConfigServer.cpp`**

```cpp
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
```

- [ ] **Step 3: Commit**

```bash
git add firmware/DeskMatrix/web/ConfigServer.h firmware/DeskMatrix/ConfigServer.cpp
git commit -m "add POST /api/screensaver upload endpoint"
```

---

## Task 6: `SpotifyService` — polling + album-art change detection

**Files:**
- Create: `firmware/DeskMatrix/services/SpotifyService.h`
- Create: `firmware/DeskMatrix/SpotifyService.cpp`
- Create: `tests/native/test_spotify_service.cpp`

**Interfaces:**
- Consumes: `SpotifyConfig` from Task 2
- Produces: `struct SpotifyStatus { bool isPlaying; std::string albumArtUrl; }`; `class SpotifyService { SpotifyService(clientId, clientSecret, refreshToken, pollSec); void loop(); SpotifyStatus latest() const; void configure(clientId, clientSecret, refreshToken, pollSec); }`; `bool albumArtChanged(const std::string& previousUrl, const std::string& newUrl)`
- Consumed by: Task 8 (`SpotifyScreen` uses `albumArtChanged()`), Task 9b (`DeskMatrix.ino` constructs/polls `SpotifyService` and drives `ScreenStateMachine::spotifyStarted()`/`spotifyStopped()` from `latest().isPlaying`)

Only `albumArtChanged()` is pure logic — the rest of this class wraps `SpotifyArduino`'s network calls, which need real Wi-Fi and a real Spotify account to exercise (same category as `WeatherService`, not natively testable). TDD the pure helper first, then write the class around it.

- [ ] **Step 1: Write the failing test for `albumArtChanged()`**

```cpp
// tests/native/test_spotify_service.cpp
#include "test_framework.h"
#include "services/SpotifyService.h"

int main() {
    CHECK(albumArtChanged("", "https://i.scdn.co/image/abc"));
    CHECK(albumArtChanged("https://i.scdn.co/image/abc", "https://i.scdn.co/image/def"));
    CHECK(!albumArtChanged("https://i.scdn.co/image/abc", "https://i.scdn.co/image/abc"));
    CHECK(!albumArtChanged("https://i.scdn.co/image/abc", "")); // new URL empty: nothing to draw, don't treat as a change

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL to compile — `services/SpotifyService.h` doesn't exist yet.

- [ ] **Step 3: Write `SpotifyService.h`**

```cpp
// firmware/DeskMatrix/services/SpotifyService.h
#pragma once
#include <string>

struct SpotifyStatus {
    bool isPlaying = false;
    std::string albumArtUrl; // empty if nothing playing / no valid reading yet
};

class SpotifyService {
public:
    SpotifyService(const std::string& clientId, const std::string& clientSecret,
                   const std::string& refreshToken, int pollSec);
    void loop(); // call every loop() iteration; internally rate-limits to pollSec
    SpotifyStatus latest() const { return latest_; }

    // Updates credentials/poll interval (e.g. after a config push) and resets
    // the poll timer so loop() polls immediately on its next call.
    void configure(const std::string& clientId, const std::string& clientSecret,
                   const std::string& refreshToken, int pollSec);

private:
    void poll();
    std::string clientId_, clientSecret_, refreshToken_;
    int pollSec_;
    unsigned long lastPollMs_ = 0;
    SpotifyStatus latest_;
};

// Pure helper: true when `newUrl` is non-empty and different from
// `previousUrl`, meaning a caller should re-fetch/re-decode album art rather
// than reuse what's already drawn.
bool albumArtChanged(const std::string& previousUrl, const std::string& newUrl);
```

- [ ] **Step 4: Write a minimal `SpotifyService.cpp` (just enough for the test to pass)**

```cpp
// firmware/DeskMatrix/SpotifyService.cpp
#include "services/SpotifyService.h"

bool albumArtChanged(const std::string& previousUrl, const std::string& newUrl) {
    return !newUrl.empty() && newUrl != previousUrl;
}

SpotifyService::SpotifyService(const std::string& clientId, const std::string& clientSecret,
                                const std::string& refreshToken, int pollSec)
    : clientId_(clientId), clientSecret_(clientSecret), refreshToken_(refreshToken), pollSec_(pollSec) {}

void SpotifyService::configure(const std::string& clientId, const std::string& clientSecret,
                                const std::string& refreshToken, int pollSec) {
    clientId_ = clientId;
    clientSecret_ = clientSecret;
    refreshToken_ = refreshToken;
    pollSec_ = pollSec;
    lastPollMs_ = 0; // force an immediate poll on the next loop()
}

void SpotifyService::loop() {
    unsigned long nowMs = millis();
    if (lastPollMs_ != 0 && (nowMs - lastPollMs_) < (unsigned long)pollSec_ * 1000UL) {
        return;
    }
    poll();
    lastPollMs_ = nowMs;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`

Note: this test file (`test_spotify_service.cpp`) is linked the same way every other native test is — against `ConfigModel.cpp` only, per `run_tests.sh`'s loop. `SpotifyService.cpp`'s `poll()` isn't implemented or called by this test, so it doesn't need to be part of the native build at all — only `SpotifyService.h`'s inline `latest()` and the free function `albumArtChanged()` are exercised. Do not add `SpotifyService.cpp` to the native build; its `poll()` (Step 6 below) uses `millis()`, `WiFi`, and other Arduino-only symbols that don't exist on the host.

Expected: `test_spotify_service` shows all checks passed.

- [ ] **Step 6: Fill in `poll()` with the real Arduino/SpotifyArduino wiring**

```cpp
// firmware/DeskMatrix/SpotifyService.cpp
#include "services/SpotifyService.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

bool albumArtChanged(const std::string& previousUrl, const std::string& newUrl) {
    return !newUrl.empty() && newUrl != previousUrl;
}

namespace {
// SpotifyArduino's getCurrentlyPlaying callback is a plain function pointer
// with no captured state, so poll()'s result is staged here and copied into
// SpotifyService::latest_ immediately after the (synchronous) call returns.
bool g_pendingIsPlaying = false;
std::string g_pendingArtUrl;

void onCurrentlyPlaying(CurrentlyPlaying currentlyPlaying) {
    g_pendingIsPlaying = currentlyPlaying.isPlaying;
    g_pendingArtUrl.clear();
    // Spotify returns album art in 3 sizes (large/medium/small); index 2 is
    // the smallest, matching this panel's 64x64 resolution — same choice
    // used by https://github.com/dylduhamel/spotify_esp32_led_matrix, which
    // this integration is modeled on.
    if (currentlyPlaying.numImages > 2 && currentlyPlaying.albumImages[2].url) {
        g_pendingArtUrl = currentlyPlaying.albumImages[2].url;
    }
}
}  // namespace

SpotifyService::SpotifyService(const std::string& clientId, const std::string& clientSecret,
                                const std::string& refreshToken, int pollSec)
    : clientId_(clientId), clientSecret_(clientSecret), refreshToken_(refreshToken), pollSec_(pollSec) {}

void SpotifyService::configure(const std::string& clientId, const std::string& clientSecret,
                                const std::string& refreshToken, int pollSec) {
    clientId_ = clientId;
    clientSecret_ = clientSecret;
    refreshToken_ = refreshToken;
    pollSec_ = pollSec;
    lastPollMs_ = 0; // force an immediate poll on the next loop()
}

void SpotifyService::loop() {
    unsigned long nowMs = millis();
    if (lastPollMs_ != 0 && (nowMs - lastPollMs_) < (unsigned long)pollSec_ * 1000UL) {
        return;
    }
    poll();
    lastPollMs_ = nowMs;
}

void SpotifyService::poll() {
    if (clientId_.empty() || clientSecret_.empty() || refreshToken_.empty()) {
        return; // not configured yet, per the design spec's error handling
    }
    if (WiFi.status() != WL_CONNECTED) return;

    static WiFiClientSecure client;
    static SpotifyArduino* spotify = nullptr;
    static std::string lastClientId, lastRefreshToken;

    // Re-create the SpotifyArduino instance whenever credentials actually
    // change (including the first call) — the library takes them by pointer
    // at construction, so it can't be reconfigured in place.
    if (!spotify || lastClientId != clientId_ || lastRefreshToken != refreshToken_) {
        delete spotify;
        client.setCACert(spotify_server_cert);
        spotify = new SpotifyArduino(client, clientId_.c_str(), clientSecret_.c_str(), refreshToken_.c_str());
        if (!spotify->refreshAccessToken()) {
            Serial.println("[spotify] failed to refresh access token");
        }
        lastClientId = clientId_;
        lastRefreshToken = refreshToken_;
    }

    g_pendingIsPlaying = false;
    g_pendingArtUrl.clear();
    int status = spotify->getCurrentlyPlaying(onCurrentlyPlaying, "");
    if (status == 200) {
        latest_.isPlaying = g_pendingIsPlaying;
        latest_.albumArtUrl = g_pendingArtUrl;
    } else if (status == 204) {
        latest_.isPlaying = false; // Spotify's contract for "nothing playing"
    } else {
        Serial.printf("[spotify] getCurrentlyPlaying failed, status=%d\n", status);
        // keep last-known-good status, per the design spec's error handling
    }
}
```

This step isn't re-verified by the native suite (it depends on Arduino/`SpotifyArduino` symbols that don't exist on the host) — it's verified in Task 9b's on-device integration pass instead.

- [ ] **Step 7: Commit**

```bash
git add firmware/DeskMatrix/services/SpotifyService.h firmware/DeskMatrix/SpotifyService.cpp tests/native/test_spotify_service.cpp
git commit -m "add SpotifyService: polling, credential config, album-art change detection"
```

---

## Task 7: `ScreensaverScreen` — GIF playback

**Files:**
- Create: `firmware/DeskMatrix/screens/ScreensaverScreen.h`
- Create: `firmware/DeskMatrix/ScreensaverScreen.cpp`

**Interfaces:**
- Produces: `bool loadScreensaverGif()`; `void drawScreensaverFrame(MatrixPanel_I2S_DMA* display)`
- Consumed by: Task 9a (`DeskMatrix.ino` calls `loadScreensaverGif()` at boot and after a screensaver upload, `drawScreensaverFrame()` every loop iteration while `mode() == SCREENSAVER`)

Not natively testable — GIF decoding and panel drawing both require the real `AnimatedGIF` library, `LittleFS`, and the HUB75 display. Verified on-device in Task 9a.

- [ ] **Step 1: Write `ScreensaverScreen.h`**

```cpp
// firmware/DeskMatrix/screens/ScreensaverScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Loads the currently-uploaded screensaver GIF from LittleFS (see
// POST /api/screensaver). Call once at boot and again right after a new
// upload. Returns false if no screensaver GIF exists yet — the caller
// should show a blank screen instead, per the design spec's error handling.
bool loadScreensaverGif();

// Advances and draws the next animation frame if this frame's display
// duration has elapsed; a cheap no-op check otherwise. Call this every main
// loop() iteration (not gated by any fixed render tick) so playback runs at
// the GIF's own frame rate rather than being throttled to once per second.
// Flips the display buffer itself whenever it actually draws a frame.
void drawScreensaverFrame(MatrixPanel_I2S_DMA* display);
```

- [ ] **Step 2: Write `ScreensaverScreen.cpp`**

```cpp
// firmware/DeskMatrix/ScreensaverScreen.cpp
//
// NOT VERIFIED AGAINST REAL HARDWARE — AnimatedGIF's exact callback
// signatures and palette-mode constant name have varied slightly between
// library releases. Confirm this compiles as-is against whatever version
// `arduino-cli lib install AnimatedGIF` pulls; expected to be a small
// signature fix (e.g. the palette-mode constant name), not a redesign — the
// same kind of one-time hardware calibration ImuHardware.cpp already needed.
#include "screens/ScreensaverScreen.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>

namespace {
const char* kScreensaverPath = "/screensaver.gif";
AnimatedGIF g_gif;
File g_gifFile;
bool g_loaded = false;
unsigned long g_nextFrameDueMs = 0;
MatrixPanel_I2S_DMA* g_display = nullptr; // set only for the duration of one playFrame() call

void* gifOpen(const char* filename, int32_t* size) {
    g_gifFile = LittleFS.open(filename, "r");
    *size = g_gifFile.size();
    return &g_gifFile;
}
void gifClose(void*) {
    if (g_gifFile) g_gifFile.close();
}
int32_t gifRead(GIFFILE*, uint8_t* buffer, int32_t length) {
    if (!g_gifFile) return 0;
    return g_gifFile.read(buffer, length);
}
int32_t gifSeek(GIFFILE*, int32_t position) {
    if (!g_gifFile) return 0;
    g_gifFile.seek(position);
    return position;
}

// Called once per decoded scanline with pre-resolved RGB565 pixels
// (gif.begin() below selects the RGB565 draw mode rather than raw palette
// indices, so no manual palette lookup is needed here).
void gifDraw(GIFDRAW* pDraw) {
    if (!g_display) return;
    uint16_t* row = (uint16_t*)pDraw->pPixels;
    for (int x = 0; x < pDraw->iWidth; x++) {
        g_display->drawPixel(pDraw->iX + x, pDraw->iY + pDraw->y, row[x]);
    }
}
}  // namespace

bool loadScreensaverGif() {
    if (!LittleFS.exists(kScreensaverPath)) {
        g_loaded = false;
        return false;
    }
    g_gif.begin(BIG_ENDIAN_PIXELS);
    g_loaded = g_gif.open(kScreensaverPath, gifOpen, gifClose, gifRead, gifSeek, gifDraw);
    g_nextFrameDueMs = 0;
    return g_loaded;
}

void drawScreensaverFrame(MatrixPanel_I2S_DMA* display) {
    if (!display || !g_loaded) return;

    unsigned long nowMs = millis();
    if (nowMs < g_nextFrameDueMs) return;

    g_display = display;
    int delayMs = 0;
    int result = g_gif.playFrame(false, &delayMs);
    g_display = nullptr;

    if (result == 0) {
        // End of the GIF's loop. AnimatedGIF rewinds automatically for most
        // files per their own loop-count metadata, but re-open defensively
        // in case this one doesn't.
        g_gif.close();
        loadScreensaverGif();
    }

    display->flipDMABuffer();
    g_nextFrameDueMs = nowMs + (delayMs > 0 ? (unsigned long)delayMs : 100UL);
}
```

- [ ] **Step 3: Commit**

```bash
git add firmware/DeskMatrix/screens/ScreensaverScreen.h firmware/DeskMatrix/ScreensaverScreen.cpp
git commit -m "add ScreensaverScreen: GIF playback from LittleFS"
```

---

## Task 8: `SpotifyScreen` — full-screen album art

**Files:**
- Create: `firmware/DeskMatrix/screens/SpotifyScreen.h`
- Create: `firmware/DeskMatrix/SpotifyScreen.cpp`

**Interfaces:**
- Consumes: `albumArtChanged()` from Task 6
- Produces: `void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl)`
- Consumed by: Task 9b (`DeskMatrix.ino` calls this every render tick while `mode() == SPOTIFY_PLAYING`, passing `spotifyService->latest().albumArtUrl`)

Not natively testable — JPEG fetch/decode requires real Wi-Fi, `WiFiClientSecure`, and `JPEGDEC`. Verified on-device in Task 9b. The dedup check itself (skip work when the URL hasn't changed) reuses the already-tested `albumArtChanged()`, so the only untested part is the actual network fetch and decode.

- [ ] **Step 1: Write `SpotifyScreen.h`**

```cpp
// firmware/DeskMatrix/screens/SpotifyScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <string>

// Draws the current Spotify album art full-screen. Only re-fetches/decodes
// the JPEG when `albumArtUrl` differs from what's already on screen (see
// albumArtChanged() in services/SpotifyService.h) — cheap no-op otherwise.
// Call this once per render tick while in ScreenMode::SPOTIFY_PLAYING.
void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl);
```

- [ ] **Step 2: Write `SpotifyScreen.cpp`**

```cpp
// firmware/DeskMatrix/SpotifyScreen.cpp
//
// NOT VERIFIED AGAINST REAL HARDWARE — JPEGDEC's in-memory-buffer open
// method name (openRAM here) has changed across library versions. Confirm
// against whatever version `arduino-cli lib install JPEGDEC` pulls; expected
// to be a small signature fix, not a redesign.
#include "screens/SpotifyScreen.h"
#include "services/SpotifyService.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SpotifyArduinoCert.h>
#include <JPEGDEC.h>
#include <vector>

namespace {
std::string g_lastDrawnUrl;
MatrixPanel_I2S_DMA* g_display = nullptr; // set only for the duration of one decode() call
JPEGDEC g_jpeg;

int onJpegDraw(JPEGDRAW* pDraw) {
    if (!g_display || pDraw->y >= g_display->height()) return 0;
    g_display->drawRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    return 1;
}
}  // namespace

void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl) {
    if (!display || !albumArtChanged(g_lastDrawnUrl, albumArtUrl)) return;

    WiFiClientSecure client;
    client.setCACert(spotify_image_server_cert);
    HTTPClient http;
    http.begin(client, albumArtUrl.c_str());
    int status = http.GET();
    if (status != 200) {
        Serial.printf("[spotify] album art fetch failed, status=%d\n", status);
        http.end();
        return; // keep whatever was drawn before, per the design spec's error handling
    }

    WiFiClient* stream = http.getStreamPtr();
    std::vector<uint8_t> jpegBytes;
    jpegBytes.reserve(8192);
    uint8_t buf[512];
    int n;
    while ((n = stream->readBytes(buf, sizeof(buf))) > 0) {
        jpegBytes.insert(jpegBytes.end(), buf, buf + n);
    }
    http.end();

    g_display = display;
    if (g_jpeg.openRAM(jpegBytes.data(), jpegBytes.size(), onJpegDraw)) {
        g_jpeg.decode(0, 0, 0);
        g_jpeg.close();
        g_lastDrawnUrl = albumArtUrl;
    } else {
        Serial.println("[spotify] JPEG decode failed");
        // keep whatever was drawn before, per the design spec's error handling
    }
    g_display = nullptr;
}
```

- [ ] **Step 3: Commit**

```bash
git add firmware/DeskMatrix/screens/SpotifyScreen.h firmware/DeskMatrix/SpotifyScreen.cpp
git commit -m "add SpotifyScreen: fetch and decode album art JPEG to the panel"
```

---

## Task 9a: Wire the screensaver into `DeskMatrix.ino`

**Files:**
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `AppConfig`/`SpotifyConfig` from Task 2 (schema only — Spotify isn't wired to anything yet), `ScreenMode`/`ScreenStateMachine` from Task 3, `loadScreensaverGif()`/`drawScreensaverFrame()` from Task 7
- Produces: a fully working device with screensaver + DND/BRB only (Spotify added in Task 9b)

First on-device checkpoint: build, flash, and confirm the screensaver plays on your real panel before Spotify is added on top. No native test applies to this file (it's wiring of already-tested pieces plus hardware calls).

- [ ] **Step 1: Rewrite `DeskMatrix.ino`**

```cpp
// firmware/DeskMatrix/DeskMatrix.ino
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFiManager.h>
#include "config.h"
#include "ConfigModel.h"
#include "SettingsStore.h"
#include "screens/ScreensaverScreen.h"
#include "ScreenStateMachine.h"
#include "TiltDebouncer.h"
#include "services/ImuHardware.h"
#include "screens/DndScreen.h"
#include "screens/BrbScreen.h"
#include "web/ConfigServer.h"

MatrixPanel_I2S_DMA *dma_display = nullptr;
WiFiManager wm;

SettingsStore settingsStore;
AppConfig appConfig;
ConfigServer configServer(appConfig, settingsStore);
ScreenStateMachine stateMachine;
TiltDebouncer tiltDebouncer(25.0f, 5);
bool imuAvailable = false;

AppConfig defaultConfig() {
  AppConfig cfg;
  cfg.spotify = SpotifyConfig{}; // empty credentials: wired up in Task 9b
  cfg.dndArt = "dnd_default";
  cfg.brbArt = "brb_default";
  return cfg;
}

void loadOrInitConfig() {
  std::string json;
  if (settingsStore.load(json)) {
    std::string error;
    if (parseConfig(json, appConfig, error)) {
      Serial.println("Loaded config from flash.");
      return;
    }
    Serial.print("Stored config invalid, using default: ");
    Serial.println(error.c_str());
  } else {
    Serial.println("No stored config found, using default.");
  }
  appConfig = defaultConfig();
  settingsStore.save(serializeConfig(appConfig));
}

void initPanel() {
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e = 9;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.double_buff = true; // avoid tearing/flicker by drawing to a back buffer and flipping once per frame

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();
}

void showIpForSeconds(const String& ip, int seconds) {
  dma_display->clearScreen();
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setCursor(2, 2);
  dma_display->setTextColor(dma_display->color565(80, 200, 255));
  dma_display->print("IP:");
  dma_display->setCursor(2, 12);
  dma_display->print(ip);
  delay((unsigned long)seconds * 1000UL);
  dma_display->clearScreen();
}

void connectWifi() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  wm.setConfigPortalTimeout(180);

  Serial.println("Starting Wi-Fi...");
  bool connected = wm.autoConnect("DeskMatrix-Setup");

  if (!connected) {
    Serial.println("Wi-Fi setup timed out. Restarting...");
    delay(2000);
    ESP.restart();
  }

  Serial.print("Wi-Fi connected. IP: ");
  Serial.println(WiFi.localIP());
  showIpForSeconds(WiFi.localIP().toString(), IP_DISPLAY_SECONDS);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  initPanel();
  connectWifi();
  settingsStore.begin();
  loadOrInitConfig();

  loadScreensaverGif(); // ok if this returns false: screensaver just shows blank until one is uploaded

  imuAvailable = imuBegin();
  if (!imuAvailable) Serial.println("IMU not found — DND/BRB disabled this boot.");

  stateMachine.wifiConfigured(); // Wi-Fi already connected above; move state machine to SCREENSAVER

  configServer.begin();
  Serial.print("Config API ready at http://");
  Serial.println(WiFi.localIP());

  configTime(TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org");
}

void loop() {
  configServer.loop();

  unsigned long nowMs = millis();

#if ENABLE_IMU_TILT
  static unsigned long lastImuPollMs = 0;
  if (imuAvailable && (nowMs - lastImuPollMs) >= 150) {
    lastImuPollMs = nowMs;
    float angle = imuReadTiltDegrees();
    TiltDirection dir = tiltDebouncer.update(angle);
    if (dir == TiltDirection::LEFT) stateMachine.tiltLeft();
    else if (dir == TiltDirection::RIGHT) stateMachine.tiltRight();
    else stateMachine.tiltCenter();
  }
#endif

  if (stateMachine.mode() == ScreenMode::SCREENSAVER) {
    // Not gated by any fixed tick: GIF playback paces itself off each
    // frame's own display duration (see ScreensaverScreen.cpp).
    drawScreensaverFrame(dma_display);
    return;
  }

  static unsigned long lastRenderMs = 0;
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    switch (stateMachine.mode()) {
      case ScreenMode::SPOTIFY_PLAYING:
        // Wired in Task 9b.
        break;
      case ScreenMode::DND:
        drawDndScreen(dma_display);
        break;
      case ScreenMode::BRB:
        drawBrbScreen(dma_display);
        break;
      case ScreenMode::INTERRUPT_TAKEOVER:
        // Scaffolded for future one-shot event pages — unused this phase.
        break;
      default:
        break;
    }
    dma_display->flipDMABuffer();
  }
}
```

- [ ] **Step 2: Install the AnimatedGIF library**

Run: `arduino-cli lib install "AnimatedGIF"`
Expected: appears in `arduino-cli lib list`.

- [ ] **Step 3: Compile**

Run (from `firmware/DeskMatrix/`):
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```
Expected: compiles cleanly. If `AnimatedGIF`'s callback signatures differ from what Task 7 wrote (flagged in that file's top-of-file comment), fix the signature to match the installed version's header — expected to be a small, mechanical fix.

- [ ] **Step 4: Flash and verify the screensaver on the real panel**

Run: `arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .`

Upload a small test GIF once the device is on the network (replace `<device-ip>`):
```bash
curl -X POST --data-binary @test.gif "http://<device-ip>/api/screensaver"
```
Expected: `{"status":"ok"}`, and the panel plays the GIF in a loop at a reasonable frame rate. Confirm a non-GIF file is rejected:
```bash
curl -X POST --data-binary @not-a-gif.txt "http://<device-ip>/api/screensaver"
```
Expected: `400` with an error body, and the previously-uploaded GIF keeps playing (not replaced). Also confirm DND/BRB still work if `ENABLE_IMU_TILT` is set to `1` and the IMU has been calibrated — otherwise this is a no-op, unchanged from before this plan.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/DeskMatrix.ino
git commit -m "wire screensaver into DeskMatrix.ino, remove Home dashboard init"
```

---

## Task 9b: Wire Spotify into `DeskMatrix.ino`

**Files:**
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `SpotifyService`/`SpotifyStatus` from Task 6, `drawSpotifyScreen()` from Task 8, `ScreenStateMachine::spotifyStarted()`/`spotifyStopped()` from Task 3

Second on-device checkpoint: adds Spotify on top of the already-verified screensaver from Task 9a, so if something's off here it's isolated to the Spotify path.

- [ ] **Step 1: Add the Spotify includes and instance**

In `firmware/DeskMatrix/DeskMatrix.ino`, add two includes after `#include "screens/ScreensaverScreen.h"`:

```cpp
#include "services/SpotifyService.h"
#include "screens/SpotifyScreen.h"
```

Add the instance pointer next to the other globals:

```cpp
SpotifyService* spotifyService = nullptr;
```

- [ ] **Step 2: Construct it in `setup()`**

Add this line right after `loadScreensaverGif();`:

```cpp
  spotifyService = new SpotifyService(appConfig.spotify.clientId, appConfig.spotify.clientSecret,
                                       appConfig.spotify.refreshToken, appConfig.spotify.pollSec);
```

- [ ] **Step 3: Poll it and drive the state machine in `loop()`**

Replace:

```cpp
void loop() {
  configServer.loop();

  unsigned long nowMs = millis();
```

with:

```cpp
void loop() {
  configServer.loop();

  if (configServer.configChanged()) {
    Serial.println("[config] change detected");
    spotifyService->configure(appConfig.spotify.clientId, appConfig.spotify.clientSecret,
                               appConfig.spotify.refreshToken, appConfig.spotify.pollSec);
  }

  spotifyService->loop(); // polls continuously regardless of current mode, so a mode switch happens promptly

  SpotifyStatus spotify = spotifyService->latest();
  if (spotify.isPlaying) {
    stateMachine.spotifyStarted();
  } else {
    stateMachine.spotifyStopped();
  }

  unsigned long nowMs = millis();
```

- [ ] **Step 4: Fill in the `SPOTIFY_PLAYING` case**

Replace:

```cpp
      case ScreenMode::SPOTIFY_PLAYING:
        // Wired in Task 9b.
        break;
```

with:

```cpp
      case ScreenMode::SPOTIFY_PLAYING:
        drawSpotifyScreen(dma_display, spotify.albumArtUrl);
        break;
```

- [ ] **Step 5: Install the remaining Arduino libraries**

Run: `arduino-cli lib install "JPEGDEC"`

`SpotifyArduino` isn't in the Library Manager index — install it from source:

```bash
cd /tmp
curl -L -o spotify-api-arduino.zip https://github.com/witnessmenow/spotify-api-arduino/archive/refs/heads/master.zip
arduino-cli lib install --zip-path spotify-api-arduino.zip
```

Expected: both appear in `arduino-cli lib list`.

- [ ] **Step 6: Compile**

Run (from `firmware/DeskMatrix/`):
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```
Expected: compiles cleanly. If `JPEGDEC`'s callback signature differs from what Task 8 wrote (flagged in that file's top-of-file comment), fix the signature to match the installed version's header — expected to be a small, mechanical fix.

- [ ] **Step 7: Flash and verify Spotify on the real panel (needs Task 10's bootstrap script run first, to push real credentials)**

Run: `arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .`

Play a song on the Spotify account the refresh token belongs to. Expected: within `pollSec` (default 5s), the panel switches from the screensaver to full-screen album art. Pause the song. Expected: within `pollSec`, the panel reverts to the screensaver.

- [ ] **Step 8: Commit**

```bash
git add firmware/DeskMatrix/DeskMatrix.ino
git commit -m "wire Spotify now-playing into DeskMatrix.ino"
```

---

## Task 10: One-time Spotify OAuth bootstrap script

**Files:**
- Create: `tools/spotify_bootstrap.py`
- Create: `tools/test_spotify_bootstrap.py`

**Interfaces:**
- Produces: `build_authorize_url(client_id, redirect_uri) -> str`; `extract_code_from_redirect(pasted_url) -> str`; `exchange_code_for_refresh_token(client_id, client_secret, code, redirect_uri) -> str`; `push_to_device(device_ip, client_id, client_secret, refresh_token, poll_sec) -> None`
- Consumed by: the user, once, to obtain the `spotify.refreshToken` value that Task 9b's `SpotifyService` needs. Never runs on the device.

This is the "small one-off local script" the design spec describes: the login happens once, in any browser (a phone is fine, per the design's chosen approach — copying the failed-redirect URL out of the browser's address bar works even though `127.0.0.1` isn't reachable from a phone). Only the pure URL-building/parsing logic is unit tested; the actual token exchange and device push are network calls verified manually.

- [ ] **Step 1: Write the failing tests**

```python
#!/usr/bin/env python3
# tools/test_spotify_bootstrap.py
import unittest
from spotify_bootstrap import build_authorize_url, extract_code_from_redirect


class TestSpotifyBootstrap(unittest.TestCase):
    def test_build_authorize_url_includes_required_params(self):
        url = build_authorize_url("abc123", "http://127.0.0.1:8888/callback")
        self.assertIn("client_id=abc123", url)
        self.assertIn("response_type=code", url)
        self.assertIn("redirect_uri=http%3A%2F%2F127.0.0.1%3A8888%2Fcallback", url)
        self.assertIn("scope=user-read-currently-playing", url)

    def test_extract_code_from_redirect_url(self):
        url = "http://127.0.0.1:8888/callback?code=AQD_someLongCode123&state=xyz"
        self.assertEqual(extract_code_from_redirect(url), "AQD_someLongCode123")

    def test_extract_code_missing_raises(self):
        with self.assertRaises(ValueError):
            extract_code_from_redirect("http://127.0.0.1:8888/callback?error=access_denied")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd tools && python3 -m unittest test_spotify_bootstrap.py -v`
Expected: FAIL — `spotify_bootstrap` module doesn't exist yet (`ModuleNotFoundError`).

- [ ] **Step 3: Write `spotify_bootstrap.py`**

```python
#!/usr/bin/env python3
# tools/spotify_bootstrap.py
"""One-time Spotify OAuth bootstrap for the Office Desk Display.

Run this once (from any machine — the login itself can happen in any
browser, including your phone's) to mint a Spotify refresh token and push it
to the device's config. After this, the device polls Spotify's API on its
own; this script is never needed again unless you revoke access.
"""
import argparse
import json
import urllib.parse
import urllib.request


def build_authorize_url(client_id: str, redirect_uri: str) -> str:
    params = {
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": redirect_uri,
        "scope": "user-read-currently-playing",
    }
    return "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode(params)


def extract_code_from_redirect(pasted_url: str) -> str:
    """Pulls the `code` query param out of the URL the browser was sent to
    after login. Works even though that URL fails to load (it points at
    127.0.0.1, unreachable from a phone) — the browser still shows the full
    URL in its address bar for the user to copy."""
    parsed = urllib.parse.urlparse(pasted_url)
    query = urllib.parse.parse_qs(parsed.query)
    if "code" not in query:
        raise ValueError(f"no 'code' parameter found in: {pasted_url}")
    return query["code"][0]


def exchange_code_for_refresh_token(client_id: str, client_secret: str, code: str, redirect_uri: str) -> str:
    data = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": redirect_uri,
        "client_id": client_id,
        "client_secret": client_secret,
    }).encode()
    req = urllib.request.Request("https://accounts.spotify.com/api/token", data=data)
    with urllib.request.urlopen(req) as resp:
        body = json.loads(resp.read())
    if "refresh_token" not in body:
        raise RuntimeError(f"token exchange failed: {body}")
    return body["refresh_token"]


def push_to_device(device_ip: str, client_id: str, client_secret: str, refresh_token: str, poll_sec: int) -> None:
    get_req = urllib.request.Request(f"http://{device_ip}/api/config")
    with urllib.request.urlopen(get_req) as resp:
        config = json.loads(resp.read())
    config["spotify"] = {
        "clientId": client_id,
        "clientSecret": client_secret,
        "refreshToken": refresh_token,
        "pollSec": poll_sec,
    }
    put_req = urllib.request.Request(
        f"http://{device_ip}/api/config",
        data=json.dumps(config).encode(),
        method="PUT",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(put_req) as resp:
        print(f"pushed to device: {resp.status} {resp.read().decode()}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-id", required=True)
    parser.add_argument("--client-secret", required=True)
    parser.add_argument("--redirect-uri", default="http://127.0.0.1:8888/callback")
    parser.add_argument("--poll-sec", type=int, default=5)
    parser.add_argument("--device-ip", help="If given, pushes the resulting config straight to the device")
    args = parser.parse_args()

    print("Open this URL in any browser (your phone is fine) and log in:\n")
    print(build_authorize_url(args.client_id, args.redirect_uri))
    print("\nAfter logging in, the browser will try to load a 127.0.0.1 page and fail — that's expected.")
    pasted = input("Copy the full URL from the browser's address bar and paste it here: ").strip()

    code = extract_code_from_redirect(pasted)
    refresh_token = exchange_code_for_refresh_token(args.client_id, args.client_secret, code, args.redirect_uri)
    print(f"\nRefresh token: {refresh_token}")

    if args.device_ip:
        push_to_device(args.device_ip, args.client_id, args.client_secret, refresh_token, args.poll_sec)


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd tools && python3 -m unittest test_spotify_bootstrap.py -v`
Expected: all 3 tests pass.

- [ ] **Step 5: Commit**

```bash
git add tools/spotify_bootstrap.py tools/test_spotify_bootstrap.py
git commit -m "add one-time Spotify OAuth bootstrap script"
```

- [ ] **Step 6: Run it for real**

Create a Spotify app at https://developer.spotify.com/dashboard (any name), add `http://127.0.0.1:8888/callback` as a Redirect URI in its settings, then:

```bash
python3 tools/spotify_bootstrap.py --client-id YOUR_CLIENT_ID --client-secret YOUR_CLIENT_SECRET --device-ip <device-ip>
```

Expected: prints the authorize URL, accepts the pasted redirect URL, prints a refresh token, and (since `--device-ip` was given) pushes it straight to the device's config — completing Task 9b Step 7's prerequisite.

---

## Self-Review Notes

- **Spec coverage:** GIF screensaver (Tasks 4/5/7/9a), Spotify now-playing (Tasks 2/6/8/9b), Home dashboard removal (Task 1), config schema (Task 2), error handling for malformed config/GIF/API failures (Tasks 2, 5, 6, 8), one-time OAuth bootstrap (Task 10) — all spec sections have a task.
- **Type consistency checked:** `SpotifyStatus`/`SpotifyConfig` field names (`isPlaying`, `albumArtUrl`, `clientId`, `clientSecret`, `refreshToken`, `pollSec`) match across Tasks 2, 6, 8, 9b. `ScreenMode::SCREENSAVER`/`SPOTIFY_PLAYING` match across Tasks 3, 9a, and 9b. `loadScreensaverGif()`/`drawScreensaverFrame()` signatures match between Task 7's header and Task 9a's call sites; `drawSpotifyScreen()` matches between Task 8's header and Task 9b's call site.
- **Deferred to real hardware, by design (matches this project's existing precedent for `WeatherService`/`ImuHardware`):** exact `AnimatedGIF`/`JPEGDEC` callback signatures (flagged in-file), IMU tilt axis calibration (pre-existing, `ENABLE_IMU_TILT` stays `0` until calibrated — unchanged by this plan), real-account Spotify polling/art rendering.
- **Execution order note:** Task 10 (the OAuth bootstrap script) is listed last but its "push to device" step is a prerequisite for Task 9b Step 7's live Spotify verification — run Task 10 before or during Task 9b, not strictly after. Tasks 1–8 have no such ordering constraint among themselves beyond the dependencies already listed in each task's "Consumes" line.
