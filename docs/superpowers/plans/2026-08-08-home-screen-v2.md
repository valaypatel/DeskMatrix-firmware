# Home Screen v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a local clock (12h + day-of-week), a remote London clock, and an icon rendering pipeline (flags + weather condition icons) to the top 64×32 strip of the Home dashboard.

**Architecture:** Every new capability splits into a pure-logic piece (native-TDD'd via `tests/native/`, no hardware) and a hardware-facing piece that wires it into the real ESP32 firmware (compile-verified + real on-device upload/check, since a board is attached and reachable at `/dev/cu.usbmodem101` for this plan — unlike the MVP plan, hardware verification is not deferred here).

**Tech Stack:** Same as the MVP platform — Arduino via `arduino-cli`, `esp32:esp32` core 3.3.11, `ESP32-HUB75-MatrixPanel-I2S-DMA`, `ArduinoJson`, `HTTPClient`, `LittleFS`.

## Global Constraints

- Board FQBN: `esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc`
- Compile from `firmware/DeskMatrix/`: `arduino-cli compile --fqbn "<above>" .`
- Upload: `arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "<above>" .`
- File placement: `.h` in `screens/`/`services/` subdirectories, `.cpp` at `firmware/DeskMatrix/` root (arduino-cli does not compile subdirectory `.cpp` files — established in the MVP plan)
- HTTP fetches to Open-Meteo MUST buffer the full response via `http.getString()` before calling `deserializeJson()` — Open-Meteo uses chunked transfer encoding, and streaming directly into `deserializeJson(http.getStream())` produces `DeserializationError::InvalidInput` (discovered and fixed live in `WeatherService.cpp` during MVP hardware bring-up — do not repeat this bug in new HTTP fetch code)
- Icons are stored as canonical 64×64 RGB565 bitmaps (8192 bytes, row-major, native `uint16_t` per pixel matching `display->color565()` output) at `/assets/<id>.bin` via the existing `POST /api/assets?id=<id>` endpoint — no API changes needed
- Native tests: `bash tests/native/run_tests.sh` from the repo root (already has correct compiler flags for this Mac)
- Never crash on a missing icon asset — draw nothing, don't block the rest of the widget

---

## Part A — Pure logic (native TDD)

### Task 1: Weather icon selection (WMO code + is_day → icon id)

**Files:**
- Create: `firmware/DeskMatrix/services/WeatherIconMap.h`
- Test: `tests/native/test_weather_icon_map.cpp`

**Interfaces:**
- Produces: `std::string weatherIconId(int wmoCode, bool isDay)` returning one of `"weather_sunny"`, `"weather_cloudy"`, `"weather_rainy"`, `"weather_clear_night"`

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_weather_icon_map.cpp
#include "test_framework.h"
#include "services/WeatherIconMap.h"

int main() {
    // Clear sky (0) and mainly clear (1): day -> sunny, night -> clear_night
    CHECK_EQ(weatherIconId(0, true), std::string("weather_sunny"));
    CHECK_EQ(weatherIconId(0, false), std::string("weather_clear_night"));
    CHECK_EQ(weatherIconId(1, true), std::string("weather_sunny"));
    CHECK_EQ(weatherIconId(1, false), std::string("weather_clear_night"));

    // Partly cloudy / overcast / fog: always cloudy, day or night
    CHECK_EQ(weatherIconId(2, true), std::string("weather_cloudy"));
    CHECK_EQ(weatherIconId(3, false), std::string("weather_cloudy"));
    CHECK_EQ(weatherIconId(45, true), std::string("weather_cloudy"));
    CHECK_EQ(weatherIconId(48, false), std::string("weather_cloudy"));

    // Drizzle, rain, rain showers, freezing rain: rainy
    CHECK_EQ(weatherIconId(51, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(61, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(65, false), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(80, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(56, true), std::string("weather_rainy"));

    // Snow and thunderstorms: no dedicated icon in this MVP's 4-icon set,
    // fall back to rainy (closest visual match — precipitation)
    CHECK_EQ(weatherIconId(71, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(85, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(95, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(99, false), std::string("weather_rainy"));

    // Unknown/unexpected code: safe fallback
    CHECK_EQ(weatherIconId(9999, true), std::string("weather_cloudy"));

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL — `WeatherIconMap.h` doesn't exist

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/services/WeatherIconMap.h
#pragma once
#include <string>

// Maps Open-Meteo's WMO weather_code (https://open-meteo.com/en/docs, "WMO
// Weather interpretation codes") plus is_day to one of this project's four
// icon ids. Snow and thunderstorm codes fall back to "rainy" since this
// MVP's icon set doesn't include dedicated snow/storm icons — precipitation
// is the closest visual match.
inline std::string weatherIconId(int wmoCode, bool isDay) {
    if (wmoCode == 0 || wmoCode == 1) {
        return isDay ? "weather_sunny" : "weather_clear_night";
    }
    if (wmoCode == 2 || wmoCode == 3 || wmoCode == 45 || wmoCode == 48) {
        return "weather_cloudy";
    }
    bool isPrecipitation =
        (wmoCode >= 51 && wmoCode <= 67) ||   // drizzle, rain, freezing rain
        (wmoCode >= 71 && wmoCode <= 77) ||   // snow
        (wmoCode >= 80 && wmoCode <= 86) ||   // showers (rain or snow)
        (wmoCode == 95 || wmoCode == 96 || wmoCode == 99); // thunderstorm
    if (isPrecipitation) {
        return "weather_rainy";
    }
    return "weather_cloudy"; // safe fallback for anything unrecognized
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `--- Running test_weather_icon_map ---` then `14/14 checks passed`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/services/WeatherIconMap.h tests/native/test_weather_icon_map.cpp
git commit -m "feat: add weather_code+is_day to icon id mapping"
```

---

### Task 2: Icon pixel-sampling math

**Files:**
- Create: `firmware/DeskMatrix/IconRenderer.h`
- Test: `tests/native/test_icon_renderer.cpp`

**Interfaces:**
- Produces: `struct IconSample { int sx; int sy; }`, `IconSample sampleIconPixel(int outX, int outY, int outW, int outH, int srcSize = 64)`

This is the nearest-neighbor scaling math `drawIcon` (Task 5) will use to read a 64×64 source bitmap down to a widget's actual size. Kept pure and hardware-independent so it's natively testable — no LittleFS, no display.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_icon_renderer.cpp
#include "test_framework.h"
#include "IconRenderer.h"

int main() {
    // Output same size as source: 1:1 mapping
    IconSample s1 = sampleIconPixel(0, 0, 64, 64);
    CHECK_EQ(s1.sx, 0); CHECK_EQ(s1.sy, 0);
    IconSample s2 = sampleIconPixel(32, 32, 64, 64);
    CHECK_EQ(s2.sx, 32); CHECK_EQ(s2.sy, 32);
    IconSample s3 = sampleIconPixel(63, 63, 64, 64);
    CHECK_EQ(s3.sx, 63); CHECK_EQ(s3.sy, 63);

    // Downscale to 32x32 (half size): output pixel N maps to source pixel 2N
    IconSample s4 = sampleIconPixel(0, 0, 32, 32);
    CHECK_EQ(s4.sx, 0); CHECK_EQ(s4.sy, 0);
    IconSample s5 = sampleIconPixel(16, 16, 32, 32);
    CHECK_EQ(s5.sx, 32); CHECK_EQ(s5.sy, 32);
    IconSample s6 = sampleIconPixel(31, 31, 32, 32);
    CHECK_EQ(s6.sx, 62); CHECK_EQ(s6.sy, 62);

    // Downscale to a small, non-power-of-two flag size (12x8)
    IconSample s7 = sampleIconPixel(0, 0, 12, 8);
    CHECK_EQ(s7.sx, 0); CHECK_EQ(s7.sy, 0);
    IconSample s8 = sampleIconPixel(11, 7, 12, 8);
    CHECK_EQ(s8.sx, 58); CHECK_EQ(s8.sy, 56); // 11*64/12=58, 7*64/8=56

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL — `IconRenderer.h` doesn't exist

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/IconRenderer.h
#pragma once

struct IconSample {
    int sx;
    int sy;
};

// Nearest-neighbor: which pixel of a `srcSize`x`srcSize` source bitmap
// should supply the color for output pixel (outX, outY) of a `outW`x`outH`
// destination. Pure integer math, no rounding surprises for our fixed
// srcSize=64 source icons.
inline IconSample sampleIconPixel(int outX, int outY, int outW, int outH, int srcSize = 64) {
    IconSample s;
    s.sx = outX * srcSize / outW;
    s.sy = outY * srcSize / outH;
    return s;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `--- Running test_icon_renderer ---` then `8/8 checks passed`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/IconRenderer.h tests/native/test_icon_renderer.cpp
git commit -m "feat: add nearest-neighbor icon pixel-sampling math"
```

---

### Task 3: ConfigModel — add `location` field to WidgetConfig

**Files:**
- Modify: `firmware/DeskMatrix/ConfigModel.h`
- Modify: `firmware/DeskMatrix/ConfigModel.cpp`
- Modify: `tests/native/test_config_model.cpp`

**Interfaces:**
- Modifies: `WidgetConfig` gains `std::string location;` (default empty — empty means "local clock" / not a remote-timezone widget)

- [ ] **Step 1: Add the failing assertions to the existing test**

Add to `tests/native/test_config_model.cpp`, inside `main()`, after the existing widget-field checks (right after the `CHECK_EQ(cfg.homeWidgets[1].dataSource, "ds_weather");` line):

```cpp
    CHECK_EQ(cfg.homeWidgets[0].location, ""); // widget without location: empty, local clock
```

And extend the `validJson` string at the top of the test (inside the `w2` weather widget's closing `}`, add a third widget) to:

```cpp
    std::string validJson = R"({
        "home": { "widgets": [
            { "id": "w1", "type": "clock", "style": "digital", "color": "#FFDE59", "x": 0, "y": 0, "w": 24, "h": 10 },
            { "id": "w2", "type": "weather", "style": "icon_temp", "icon": "weather_sunny", "color": "#4C9AFF", "x": 0, "y": 40, "w": 16, "h": 16, "dataSource": "ds_weather" },
            { "id": "w4", "type": "clock", "style": "digital", "color": "#FFDE59", "location": "51.5074,-0.1278", "x": 0, "y": 16, "w": 32, "h": 16 }
        ]},
        "dataSources": {
            "ds_weather": { "type": "weather", "pollSec": 600, "location": "40.7128,-74.0060" }
        },
        "dnd": { "art": "dnd_default" },
        "brb": { "art": "brb_default" }
    })";
```

And add, after the round-trip check block:

```cpp
    CHECK_EQ(cfg.homeWidgets[2].location, "51.5074,-0.1278");
    CHECK_EQ(reparsed.homeWidgets[2].location, cfg.homeWidgets[2].location); // round-trips
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL — compile error, `WidgetConfig` has no member `location`

- [ ] **Step 3: Add the field to the struct**

In `firmware/DeskMatrix/ConfigModel.h`, add to `WidgetConfig`:

```cpp
struct WidgetConfig {
    std::string id;
    std::string type;
    std::string style;
    std::string color;
    std::string icon;
    int x = 0, y = 0, w = 0, h = 0;
    std::string dataSource;
    std::string location; // non-empty => remote-timezone clock; empty => local clock
};
```

- [ ] **Step 4: Parse and serialize it**

In `firmware/DeskMatrix/ConfigModel.cpp`'s `parseConfig`, inside the widget-parsing loop, add alongside the existing `wc.dataSource = ...` line:

```cpp
        wc.location = std::string(w["location"] | "");
```

In `serializeConfig`, inside the widget-serializing loop, add alongside the existing `if (!w.dataSource.empty()) wo["dataSource"] = w.dataSource;` line:

```cpp
        if (!w.location.empty()) wo["location"] = w.location;
```

- [ ] **Step 5: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `--- Running test_config_model ---` then all checks passed (verify the new count printed — should be 2 more than before)

- [ ] **Step 6: Commit**

```bash
git add firmware/DeskMatrix/ConfigModel.h firmware/DeskMatrix/ConfigModel.cpp tests/native/test_config_model.cpp
git commit -m "feat: add WidgetConfig.location for remote-timezone clocks"
```

---

### Task 4: Remote-clock time computation

**Files:**
- Create: `firmware/DeskMatrix/services/RemoteClockService.h`
- Test: `tests/native/test_remote_clock_time.cpp`

**Interfaces:**
- Produces: `struct tm computeRemoteTm(time_t utcEpoch, long offsetSec)`

Only the pure time math goes in this task — the HTTP-fetching `RemoteClockService` class that determines `offsetSec` from Open-Meteo comes in Task 6 (hardware-dependent), which extends this same header.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_remote_clock_time.cpp
#include "test_framework.h"
#include "services/RemoteClockService.h"

int main() {
    // 2026-01-15 12:00:00 UTC = epoch 1768478400 (a fixed, known reference instant)
    time_t utcNoon = 1768478400;

    // UTC+0: identical to input
    struct tm t0 = computeRemoteTm(utcNoon, 0);
    CHECK_EQ(t0.tm_hour, 12);
    CHECK_EQ(t0.tm_min, 0);

    // UTC+5:30 (India, as a sanity check against a known real offset)
    struct tm tIst = computeRemoteTm(utcNoon, 5 * 3600 + 30 * 60);
    CHECK_EQ(tIst.tm_hour, 17);
    CHECK_EQ(tIst.tm_min, 30);

    // UTC+0 (London GMT, winter/no DST) — same as t0 in this scenario
    struct tm tLondonWinter = computeRemoteTm(utcNoon, 0);
    CHECK_EQ(tLondonWinter.tm_hour, 12);

    // UTC+1 (London BST, summer DST) — offset is just a number, DST handling
    // lives entirely in what Open-Meteo's timezone=auto reports, not here
    struct tm tLondonSummer = computeRemoteTm(utcNoon, 3600);
    CHECK_EQ(tLondonSummer.tm_hour, 13);

    // Negative offset (US Eastern, UTC-5)
    struct tm tUsEast = computeRemoteTm(utcNoon, -5 * 3600);
    CHECK_EQ(tUsEast.tm_hour, 7);

    // Crossing midnight forward (large positive offset pushes into next day)
    time_t utcLateEvening = utcNoon + 13 * 3600; // 2026-01-16 01:00:00 UTC... actually let's use a value near day boundary explicitly:
    time_t utc2300 = utcNoon + 11 * 3600; // 2026-01-15 23:00:00 UTC
    struct tm tNextDay = computeRemoteTm(utc2300, 2 * 3600); // +2h -> 01:00 next day
    CHECK_EQ(tNextDay.tm_hour, 1);
    CHECK_EQ(tNextDay.tm_mday, 16);

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/native/run_tests.sh`
Expected: FAIL — `services/RemoteClockService.h` doesn't exist

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/services/RemoteClockService.h
#pragma once
#include <ctime>

// Computes the calendar/time breakdown for a UTC epoch shifted by a fixed
// offset. `time(nullptr)` on the device always returns UTC regardless of
// what configTime()'s offset is set to (that only affects localtime_r()),
// so this lets a "remote" clock show a different timezone than whatever
// the device's own configTime() is currently set to, without needing a
// full timezone database — the caller supplies the offset (from Open-Meteo's
// timezone=auto, fetched by RemoteClockService in a later task).
inline struct tm computeRemoteTm(time_t utcEpoch, long offsetSec) {
    time_t shifted = utcEpoch + offsetSec;
    struct tm result;
    gmtime_r(&shifted, &result); // gmtime_r applies NO further offset — shifted is now the "local" instant
    return result;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `--- Running test_remote_clock_time ---` then `9/9 checks passed`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/services/RemoteClockService.h tests/native/test_remote_clock_time.cpp
git commit -m "feat: add computeRemoteTm for offset-shifted remote clock time"
```

---

## Part B — On-device firmware

A physical board is attached at `/dev/cu.usbmodem101` for this plan. Each task below compiles, uploads, and gets a real on-device check via serial log evidence (HTTP status codes, parsed values, etc.) — full visual confirmation on the panel happens once at final integration (Task 9) and again at end-to-end verification (Task 11), not on every intermediate task, to avoid asking for a visual check on every single wiring step.

### Task 5: IconRenderer — on-device bitmap drawing

**Files:**
- Modify: `firmware/DeskMatrix/IconRenderer.h`
- Create: `firmware/DeskMatrix/IconRenderer.cpp`

**Interfaces:**
- Consumes: `sampleIconPixel` (Task 2)
- Produces: `void drawIcon(MatrixPanel_I2S_DMA* display, const std::string& iconId, int x, int y, int w, int h)`

- [ ] **Step 1: Add the drawIcon declaration to the header**

Add to `firmware/DeskMatrix/IconRenderer.h` (the pure `sampleIconPixel`/`IconSample` from Task 2 stay as-is — this is additive):

```cpp
#include <string>

class MatrixPanel_I2S_DMA; // forward-declared: keeps this header includable
                            // from native tests without pulling in Arduino headers

// Draws a stored icon asset (see POST /api/assets) into the given region,
// nearest-neighbor scaled from its native 64x64 to w x h. Does nothing
// (no crash) if the asset doesn't exist or isn't the expected size.
void drawIcon(MatrixPanel_I2S_DMA* display, const std::string& iconId, int x, int y, int w, int h);
```

- [ ] **Step 2: Write the implementation**

```cpp
// firmware/DeskMatrix/IconRenderer.cpp
#include "IconRenderer.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <LittleFS.h>
#include <vector>

void drawIcon(MatrixPanel_I2S_DMA* display, const std::string& iconId, int x, int y, int w, int h) {
    if (!display || w <= 0 || h <= 0) return;

    std::string path = "/assets/" + iconId + ".bin";
    if (!LittleFS.exists(path.c_str())) return;

    File f = LittleFS.open(path.c_str(), "r");
    if (!f) return;

    const size_t kExpectedBytes = 64 * 64 * 2; // 64x64 RGB565, 2 bytes/pixel
    if ((size_t)f.size() != kExpectedBytes) {
        f.close();
        return;
    }

    std::vector<uint16_t> buffer(64 * 64);
    f.read(reinterpret_cast<uint8_t*>(buffer.data()), kExpectedBytes);
    f.close();

    for (int oy = 0; oy < h; oy++) {
        for (int ox = 0; ox < w; ox++) {
            IconSample s = sampleIconPixel(ox, oy, w, h);
            uint16_t color = buffer[s.sy * 64 + s.sx];
            display->drawPixel(x + ox, y + oy, color);
        }
    }
}
```

- [ ] **Step 3: Compile**

Run (from `firmware/DeskMatrix/`):
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```
Expected: 0 errors

- [ ] **Step 4: On-device check with a synthetic test asset**

Generate an 8192-byte solid-color test bitmap and upload it (run from anywhere with `python3` and network access to the device):

```bash
python3 -c "
import sys
color = 0xF800  # red in RGB565
data = bytearray()
for _ in range(64*64):
    data += color.to_bytes(2, 'little')
sys.stdout.buffer.write(data)
" > /tmp/test_icon.bin

curl -X POST "http://10.0.0.128/api/assets?id=test_icon" --data-binary @/tmp/test_icon.bin
```
(Replace `10.0.0.128` with the device's current IP from its serial log if it's changed.)

Temporarily add one line to `DeskMatrix.ino`'s `setup()`, right after `stateMachine.wifiConfigured();`, to prove `drawIcon` executes without crashing and draws something:
```cpp
  drawIcon(dma_display, "test_icon", 0, 0, 16, 16);
```
(This is temporary verification code — remove it before committing this task; Task 9 will add the real, permanent call sites.)

Compile and upload:
```bash
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```

Confirm via serial monitor that the device boots normally with no crash/reboot loop after this point in `setup()` — that's sufficient evidence `drawIcon` read the file and wrote pixels without faulting. (A visual check that a red square actually appears is a nice bonus if convenient, but not required for this task — full visual confirmation is Task 9/11's job.)

Remove the temporary `drawIcon(...)` line from `DeskMatrix.ino` before the next step.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/IconRenderer.h firmware/DeskMatrix/IconRenderer.cpp
git commit -m "feat: add drawIcon to read and blit stored icon assets"
```

---

### Task 6: RemoteClockService — on-device Open-Meteo offset fetch

**Files:**
- Modify: `firmware/DeskMatrix/services/RemoteClockService.h`
- Create: `firmware/DeskMatrix/RemoteClockService.cpp`

**Interfaces:**
- Consumes: `computeRemoteTm` (Task 4, unchanged)
- Produces: `class RemoteClockService` with constructor `(float latitude, float longitude, unsigned long pollIntervalMs)`, `void loop()`, `bool hasOffset() const`, `long offsetSeconds() const`

- [ ] **Step 1: Add the class declaration to the header**

Add to `firmware/DeskMatrix/services/RemoteClockService.h` (the pure `computeRemoteTm` from Task 4 stays as-is):

```cpp
class RemoteClockService {
public:
    RemoteClockService(float latitude, float longitude, unsigned long pollIntervalMs);
    void loop(); // call every loop() iteration; internally rate-limits to pollIntervalMs

    bool hasOffset() const { return haveOffset_; }
    long offsetSeconds() const { return offsetSec_; }

private:
    void fetch();
    float lat_, lon_;
    unsigned long pollIntervalMs_;
    unsigned long lastFetchMs_ = 0;
    long offsetSec_ = 0;
    bool haveOffset_ = false;
};
```

- [ ] **Step 2: Write the implementation**

```cpp
// firmware/DeskMatrix/RemoteClockService.cpp
#include "services/RemoteClockService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

RemoteClockService::RemoteClockService(float latitude, float longitude, unsigned long pollIntervalMs)
    : lat_(latitude), lon_(longitude), pollIntervalMs_(pollIntervalMs) {}

void RemoteClockService::loop() {
    unsigned long nowMs = millis();
    if (lastFetchMs_ != 0 && (nowMs - lastFetchMs_) < pollIntervalMs_) {
        return;
    }
    fetch();
    lastFetchMs_ = nowMs;
}

void RemoteClockService::fetch() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    char url[224];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m&timezone=auto",
        lat_, lon_);
    http.begin(url);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return;
    }

    // Buffer the full body before parsing — Open-Meteo uses chunked transfer
    // encoding, and streaming straight from http.getStream() into
    // deserializeJson() reliably fails with InvalidInput (same issue found
    // and fixed in WeatherService.cpp during MVP bring-up).
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return;

    offsetSec_ = doc["utc_offset_seconds"] | offsetSec_;
    haveOffset_ = true;
}
```

- [ ] **Step 3: Compile**

Run the standard compile command. Expected: 0 errors.

- [ ] **Step 4: On-device check**

This task's service isn't wired into `loop()` yet (that's Task 9), so verify it directly with a temporary block in `setup()`, right after `configServer.begin();`:

```cpp
  RemoteClockService testRemote(51.5074f, -0.1278f, 6UL * 3600UL * 1000UL);
  testRemote.loop();
  delay(3000); // give the HTTP fetch time to complete before printing
  Serial.printf("[remote-clock-test] hasOffset=%d offsetSeconds=%ld\n",
                testRemote.hasOffset(), testRemote.offsetSeconds());
```

Compile, upload, and watch the serial monitor. Expected: `hasOffset=1` and `offsetSeconds=0` (GMT, winter) or `offsetSeconds=3600` (BST, summer) depending on the current date — either is correct, both mean the fetch and parse worked. Confirm this exact log line appears with `hasOffset=1` before proceeding.

Remove this temporary block from `setup()` before committing.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/services/RemoteClockService.h firmware/DeskMatrix/RemoteClockService.cpp
git commit -m "feat: add RemoteClockService fetching timezone offset from Open-Meteo"
```

---

### Task 7: ClockWidget — 12h format, day-of-week, remote clock, flag icon

**Files:**
- Modify: `firmware/DeskMatrix/ClockWidget.cpp`
- Modify: `firmware/DeskMatrix/screens/ClockWidget.h`

**Interfaces:**
- Consumes: `computeRemoteTm`, `RemoteClockService` (Task 6), `drawIcon` (Task 5), `WidgetConfig::location` (Task 3)
- Modifies: `registerClockWidget` signature changes from `(WidgetRegistry&)` to `(WidgetRegistry&, RemoteClockService&)`

- [ ] **Step 1: Update the header signature**

```cpp
// firmware/DeskMatrix/screens/ClockWidget.h
#pragma once
#include "../WidgetRegistry.h"
#include "../services/RemoteClockService.h"

void registerClockWidget(WidgetRegistry& registry, RemoteClockService& remoteClockService);
```

- [ ] **Step 2: Rewrite the draw function**

```cpp
// firmware/DeskMatrix/ClockWidget.cpp
#include "screens/ClockWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include <cstdlib>
#include "ColorUtil.h"
#include "IconRenderer.h"

static const char* kDayNames[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void registerClockWidget(WidgetRegistry& registry, RemoteClockService& remoteClockService) {
    registry.registerType("clock", [&remoteClockService](const RenderContext& ctx) {
        auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
        if (!display || !ctx.widget) return;

        struct tm timeInfo;
        bool isRemote = !ctx.widget->location.empty();
        bool showDay = false;

        if (isRemote) {
            time_t nowUtc = time(nullptr);
            if (remoteClockService.hasOffset()) {
                timeInfo = computeRemoteTm(nowUtc, remoteClockService.offsetSeconds());
            } else {
                // No offset fetched yet: show the device's own local time
                // rather than a blank/garbage clock until the first fetch lands.
                localtime_r(&nowUtc, &timeInfo);
            }
        } else {
            time_t now = time(nullptr);
            localtime_r(&now, &timeInfo);
            showDay = (ctx.widget->style == "digital_with_day");
        }

        int hour12 = timeInfo.tm_hour % 12;
        if (hour12 == 0) hour12 = 12;
        const char* ampm = (timeInfo.tm_hour < 12) ? "AM" : "PM";

        char buf[10];
        snprintf(buf, sizeof(buf), "%d:%02d%s", hour12, timeInfo.tm_min, ampm);

        uint16_t color = display->color565(255, 222, 89);
        uint8_t r, g, b;
        if (parseHexColor(ctx.widget->color, r, g, b)) {
            color = display->color565(r, g, b);
        }

        // Flag icon, if configured, sits just left of the time text. Exact
        // offset/size (12x8 here) is a starting point — confirm/adjust by
        // looking at the real panel once a real flag asset is uploaded
        // (Task 10), since legibility at this scale can only really be
        // judged on the actual hardware.
        int textX = ctx.widget->x;
        if (!ctx.widget->icon.empty()) {
            drawIcon(display, ctx.widget->icon, ctx.widget->x, ctx.widget->y, 12, 8);
            textX = ctx.widget->x + 14;
        }

        display->setTextSize(1);
        display->setTextColor(color);
        display->setCursor(textX, ctx.widget->y);
        display->print(buf);

        if (showDay) {
            display->setCursor(ctx.widget->x, ctx.widget->y + 8);
            display->print(kDayNames[timeInfo.tm_wday]);
        }
    });
}
```

- [ ] **Step 3: Compile**

Standard compile command. Expected: 0 errors. (This will show an error at the `registerClockWidget(widgetRegistry);` call site in `DeskMatrix.ino` since the signature changed — that's expected and gets fixed in Task 9. If you want this task to compile in isolation, temporarily comment out that one call site line and the matching include won't be an issue; otherwise proceed straight to Task 9 which fixes the call site as part of its own wiring work. Either is fine — note which you did in your report.)

- [ ] **Step 4: Commit**

```bash
git add firmware/DeskMatrix/ClockWidget.cpp firmware/DeskMatrix/screens/ClockWidget.h
git commit -m "feat: ClockWidget supports 12h format, day-of-week, remote timezone, flag icon"
```

---

### Task 8: WeatherWidget + WeatherService — is_day and weather icon

**Files:**
- Modify: `firmware/DeskMatrix/services/WeatherService.h`
- Modify: `firmware/DeskMatrix/WeatherService.cpp`
- Modify: `firmware/DeskMatrix/WeatherWidget.cpp`

**Interfaces:**
- Consumes: `weatherIconId` (Task 1), `drawIcon` (Task 5)
- Modifies: `WeatherReading` gains `bool isDay = true;`

- [ ] **Step 1: Add isDay to WeatherReading**

In `firmware/DeskMatrix/services/WeatherService.h`:
```cpp
struct WeatherReading {
    float temperatureC = 0;
    int weatherCode = 0;
    bool isDay = true;
    bool valid = false;
};
```

- [ ] **Step 2: Request and parse is_day**

In `firmware/DeskMatrix/WeatherService.cpp`'s `fetch()`, update the URL's `current` parameter list and add the parse line alongside the existing `weatherCode` line:

```cpp
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code,is_day&timezone=auto",
```
```cpp
    latest_.isDay = (doc["current"]["is_day"] | 1) != 0;
```
(place this line next to the existing `latest_.weatherCode = ...` assignment)

- [ ] **Step 3: Draw the weather icon**

In `firmware/DeskMatrix/WeatherWidget.cpp`, add the includes and the icon draw call:

```cpp
#include "IconRenderer.h"
#include "services/WeatherIconMap.h"
```

Inside the registered draw lambda, after the existing temperature `display->print(buf);` call (and only when `r.valid`, i.e. inside the same branch that prints the temperature rather than `"--"`), add:

```cpp
        std::string iconId = weatherIconId(r.weatherCode, r.isDay);
        // Icon fills most of the widget's cell above the temperature text.
        // Exact size/position is a starting point — confirm on the real
        // panel once a real weather icon asset is uploaded (Task 10).
        drawIcon(display, iconId, ctx.widget->x, ctx.widget->y, ctx.widget->w, ctx.widget->w > 12 ? ctx.widget->w - 8 : ctx.widget->w);
```

- [ ] **Step 4: Compile**

Standard compile command. Expected: 0 errors.

- [ ] **Step 5: On-device check**

Upload and watch serial output for the existing `[weather] parsed OK: temp=...C code=...` line (from the debug logging already in `WeatherService.cpp` from MVP bring-up) — confirm it still appears with no new errors, meaning `is_day` parsing didn't break the existing fetch/parse path.

- [ ] **Step 6: Commit**

```bash
git add firmware/DeskMatrix/services/WeatherService.h firmware/DeskMatrix/WeatherService.cpp firmware/DeskMatrix/WeatherWidget.cpp
git commit -m "feat: WeatherService tracks is_day, WeatherWidget draws condition icon"
```

---

### Task 9: DeskMatrix.ino — wire everything together, update default layout

**Files:**
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `RemoteClockService` (Task 6), updated `registerClockWidget` signature (Task 7)

- [ ] **Step 1: Add the RemoteClockService global and include**

Add near the top, alongside the existing `#include "services/WeatherService.h"`:
```cpp
#include "services/RemoteClockService.h"
```

Add near the other globals (alongside `WeatherService* weatherService = nullptr;`):
```cpp
RemoteClockService remoteClockService(51.5074f, -0.1278f, 6UL * 3600UL * 1000UL); // London, poll every 6h
```

- [ ] **Step 2: Fix the registerClockWidget call site**

Change:
```cpp
  registerClockWidget(widgetRegistry);
```
to:
```cpp
  registerClockWidget(widgetRegistry, remoteClockService);
```

- [ ] **Step 3: Poll the remote clock service in loop()**

Add alongside the existing `weatherService->loop();` line in `loop()`:
```cpp
  remoteClockService.loop();
```

- [ ] **Step 4: Update defaultConfig() to the new 4-widget layout**

Replace the existing `defaultConfig()` function body with:

```cpp
AppConfig defaultConfig() {
  AppConfig cfg;

  WidgetConfig localClock;
  localClock.id = "w1"; localClock.type = "clock"; localClock.style = "digital_with_day";
  localClock.color = "#FFDE59"; localClock.icon = "flag_in";
  localClock.x = 0; localClock.y = 0; localClock.w = 32; localClock.h = 16;
  cfg.homeWidgets.push_back(localClock);

  WidgetConfig remoteClock;
  remoteClock.id = "w4"; remoteClock.type = "clock"; remoteClock.style = "digital";
  remoteClock.color = "#FFDE59"; remoteClock.icon = "flag_gb";
  remoteClock.location = "51.5074,-0.1278"; // London
  remoteClock.x = 0; remoteClock.y = 16; remoteClock.w = 32; remoteClock.h = 16;
  cfg.homeWidgets.push_back(remoteClock);

  WidgetConfig weather;
  weather.id = "w2"; weather.type = "weather"; weather.style = "icon_temp";
  weather.icon = "weather_auto"; weather.color = "#4C9AFF";
  weather.x = 32; weather.y = 0; weather.w = 32; weather.h = 32;
  weather.dataSource = "ds_weather";
  cfg.homeWidgets.push_back(weather);

  DataSourceConfig ds;
  ds.id = "ds_weather"; ds.type = "weather"; ds.pollSec = 600;
  ds.location = "40.7128,-74.0060"; // MVP default; overwritten by real config once pushed
  cfg.dataSources.push_back(ds);

  return cfg;
}
```

Note: this only affects a **fresh or corrupted** config — the device currently has a real config in flash from earlier manual testing (Vadodara coordinates, old 2-widget layout). This default won't retroactively change what's already stored; if you want the new layout live immediately, push it via `PUT /api/config` after flashing (see Task 11).

- [ ] **Step 5: Compile**

Standard compile command. Expected: 0 errors.

- [ ] **Step 6: Upload and check serial boot log**

```bash
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```

Watch serial output for a clean boot: Wi-Fi connects, `Loaded config from flash.` (the existing real config, not the new default — expected), `Config API ready at http://...`, and the existing `[weather] ...` debug lines still firing normally. No crashes, no reboot loops.

- [ ] **Step 7: Commit**

```bash
git add firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: wire RemoteClockService and new default dual-clock+weather layout into DeskMatrix.ino"
```

---

### Task 10: Prepare and upload real icon assets (controller-executed)

This task is executed directly by whoever is running this plan (not dispatched to a subagent) — it involves sourcing/creating visual assets and getting the human's approval on how they look, which needs a human in the loop at the point of decision, not after.

**Files:** none (no firmware code changes — this uploads data to the running device)

- [ ] **Step 1: Source and convert the two flag icons**

India and UK flags, from the MIT-licensed `lipis/flag-icons` project:
```bash
mkdir -p /tmp/icon-assets
curl -sL https://cdn.jsdelivr.net/gh/lipis/flag-icons/flags/4x3/in.svg -o /tmp/icon-assets/flag_in.svg
curl -sL https://cdn.jsdelivr.net/gh/lipis/flag-icons/flags/4x3/gb.svg -o /tmp/icon-assets/flag_gb.svg
```

Rasterize each to 64×64 and convert to raw RGB565 bytes (reusing the same ImageMagick + Python pipeline already used for the Jira/fire icons earlier in this project):
```bash
for id in flag_in flag_gb; do
  magick -background white /tmp/icon-assets/$id.svg -resize 64x64 /tmp/icon-assets/$id.png
done
```

```python
# /tmp/icon-assets/to_rgb565.py — run once per icon: python3 to_rgb565.py <name>
import sys
from PIL import Image

name = sys.argv[1]
img = Image.open(f"/tmp/icon-assets/{name}.png").convert("RGB").resize((64, 64))
out = bytearray()
for y in range(64):
    for x in range(64):
        r, g, b = img.getpixel((x, y))
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out += rgb565.to_bytes(2, "little")
with open(f"/tmp/icon-assets/{name}.bin", "wb") as f:
    f.write(out)
print(f"wrote {len(out)} bytes")
```

```bash
python3 /tmp/icon-assets/to_rgb565.py flag_in
python3 /tmp/icon-assets/to_rgb565.py flag_gb
```

Each output file must be exactly 8192 bytes (`ls -la /tmp/icon-assets/*.bin` to confirm).

- [ ] **Step 2: Generate the four weather icons directly (no external SVG dependency)**

Draw simple, recognizable pixel-art shapes programmatically rather than depending on an external icon set's licensing/URL stability:

```python
# /tmp/icon-assets/make_weather_icons.py
from PIL import Image, ImageDraw

def save(img, name):
    img = img.resize((64, 64))
    out = bytearray()
    for y in range(64):
        for x in range(64):
            r, g, b, _ = img.getpixel((x, y))
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            out += rgb565.to_bytes(2, "little")
    with open(f"/tmp/icon-assets/{name}.bin", "wb") as f:
        f.write(out)
    print(f"{name}: wrote {len(out)} bytes")

# Sunny: yellow filled circle with rays
img = Image.new("RGBA", (64, 64), (0, 0, 0, 255))
d = ImageDraw.Draw(img)
d.ellipse((16, 16, 48, 48), fill=(255, 200, 0, 255))
for angle_pts in [(32,4,32,14),(32,50,32,60),(4,32,14,32),(50,32,60,32),
                   (12,12,20,20),(44,44,52,52),(12,52,20,44),(44,20,52,12)]:
    d.line(angle_pts, fill=(255, 200, 0, 255), width=3)
save(img, "weather_sunny")

# Cloudy: light gray overlapping circles
img = Image.new("RGBA", (64, 64), (10, 10, 20, 255))
d = ImageDraw.Draw(img)
d.ellipse((10, 26, 34, 50), fill=(180, 180, 190, 255))
d.ellipse((26, 18, 54, 46), fill=(200, 200, 210, 255))
d.ellipse((18, 30, 50, 54), fill=(190, 190, 200, 255))
save(img, "weather_cloudy")

# Rainy: gray cloud + blue diagonal rain lines
img = Image.new("RGBA", (64, 64), (5, 5, 15, 255))
d = ImageDraw.Draw(img)
d.ellipse((10, 14, 34, 34), fill=(140, 140, 150, 255))
d.ellipse((26, 8, 50, 32), fill=(150, 150, 160, 255))
for x in range(16, 50, 8):
    d.line((x, 38, x - 6, 58), fill=(80, 160, 255, 255), width=3)
save(img, "weather_rainy")

# Clear night: crescent moon on dark background
img = Image.new("RGBA", (64, 64), (2, 2, 12, 255))
d = ImageDraw.Draw(img)
d.ellipse((16, 12, 48, 44), fill=(220, 220, 200, 255))
d.ellipse((24, 10, 54, 40), fill=(2, 2, 12, 255)) # bite out of the circle to make a crescent
save(img, "weather_clear_night")
```

```bash
python3 /tmp/icon-assets/make_weather_icons.py
```

- [ ] **Step 3: Preview all six icons for approval**

Use the `mcp__visualize__show_widget` tool to render all six 64×64 bitmaps at a legible zoom (following this project's established pattern from the earlier Jira/fire icon work — LED-dot style on a dark background) side by side, labeled. Show this to the user and get explicit approval before uploading. If any icon needs adjustment, edit the generation script and re-render before proceeding — don't upload unapproved icons.

- [ ] **Step 4: Upload each approved icon to the device**

```bash
for id in flag_in flag_gb weather_sunny weather_cloudy weather_rainy weather_clear_night; do
  curl -X POST "http://10.0.0.128/api/assets?id=$id" --data-binary @/tmp/icon-assets/$id.bin
  echo " -- uploaded $id"
done
```
(Replace `10.0.0.128` with the device's current IP if it's changed.)

- [ ] **Step 5: Report**

No git commit for this task (nothing in the repo changes) — just confirm in your response to the human that all six assets uploaded successfully (each `curl` should return `{"status":"ok"}`).

---

### Task 11: End-to-end on-device verification

**Files:** none (verification only)

- [ ] **Step 1: Push the new layout live**

```bash
curl -X PUT http://10.0.0.128/api/config \
  -H "Content-Type: application/json" \
  --data @- <<'EOF'
{
  "home": {
    "widgets": [
      { "id": "w1", "type": "clock", "style": "digital_with_day", "color": "#FFDE59", "icon": "flag_in", "x": 0, "y": 0, "w": 32, "h": 16 },
      { "id": "w4", "type": "clock", "style": "digital", "color": "#FFDE59", "icon": "flag_gb", "location": "51.5074,-0.1278", "x": 0, "y": 16, "w": 32, "h": 16 },
      { "id": "w2", "type": "weather", "style": "icon_temp", "icon": "weather_auto", "color": "#4C9AFF", "x": 32, "y": 0, "w": 32, "h": 32, "dataSource": "ds_weather" }
    ]
  },
  "dataSources": {
    "ds_weather": { "type": "weather", "pollSec": 600, "location": "22.2994,73.2081" }
  },
  "dnd": { "art": "dnd_default" },
  "brb": { "art": "brb_default" }
}
EOF
```

- [ ] **Step 2: Ask the human to look at the panel and confirm:**

1. Top-left shows local (Vadodara/IST) time in 12h format with AM/PM, India flag, and day-of-week underneath
2. Below it, remote (London) time in 12h format with AM/PM and the UK flag — correct relative to local (London is behind India by 4.5-5.5 hours depending on DST)
3. Right side shows a weather icon matching current conditions (sunny/cloudy/rainy/clear-night) plus temperature
4. Bottom half of the panel is empty (untouched, as scoped)
5. Flag and icon positions/sizes are legible — note any that need repositioning (the exact pixel offsets in Tasks 7/8 were starting points, not final)

- [ ] **Step 3: Confirm all six native test suites still pass**

```bash
bash tests/native/run_tests.sh
```
Expected: all suites pass, including the three new ones from this plan (`test_weather_icon_map`, `test_icon_renderer`, `test_remote_clock_time`)

- [ ] **Step 4: Commit final state**

```bash
git add -A
git commit -m "chore: Home screen v2 complete — dual clocks, day display, icon rendering verified end-to-end on device"
```
