# LED Matrix Display Platform (MVP) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the ESP32-side LED matrix display platform: a Home dashboard (Clock + Weather widgets), IMU-triggered DND/BRB modes, and an HTTP config/asset/OTA API — per `docs/superpowers/specs/2026-08-08-led-matrix-platform-design.md`.

**Architecture:** Pure logic (state machine, interrupt queue, config model, tilt debounce) is written as portable C++ with no Arduino dependency and TDD'd natively (plain g++, no test framework needed). Hardware-dependent pieces (panel rendering, Wi-Fi, IMU, HTTP server, flash storage) build on top, verified on the real ESP32-S3-RGB-Matrix board since there's no meaningful way to test them off-hardware.

**Tech Stack:** Arduino framework via `arduino-cli`, `esp32:esp32` core 3.3.11, `ESP32-HUB75-MatrixPanel-I2S-DMA`, `WiFiManager`, `ArduinoJson`, built-in `WebServer`/`Update`/`LittleFS`. Board FQBN: `esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc` (switched to the LittleFS-flavored partition scheme so `POST /api/assets` and config persistence have a filesystem to write to).

## Global Constraints

- Board: Waveshare ESP32-S3-RGB-Matrix (ESP32-S3-N32R16), 64×64 HUB75 panel, connected at `/dev/cu.usbmodem101`
- Compile/upload: `arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .` and `arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "<same>" .`, both run from inside `firmware/DeskMatrix/`
- Native (host) tests compiled with `g++ -std=c++17`, no external test framework — see Task 1
- Config persists as JSON at `/config.json` on LittleFS; icon assets at `/assets/<id>.bin`
- Never crash/brick on bad input: malformed `PUT /api/config` is rejected (400), last-valid config stays active
- MVP scope only: Clock + Weather widgets, `screens` array stays empty (no Jira/Calendar yet — that's future work per the spec)

---

## Part A — Pure logic (native TDD)

### Task 1: Native test harness

**Files:**
- Create: `tests/native/test_framework.h`
- Create: `tests/native/test_smoke.cpp`
- Create: `tests/native/run_tests.sh`

**Interfaces:**
- Produces: `CHECK(cond)`, `CHECK_EQ(a, b)`, `TEST_SUMMARY()` macros used by every later native test file

- [ ] **Step 1: Write the test framework header**

```cpp
// tests/native/test_framework.h
#pragma once
#include <cstdio>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond) do { \
    g_tests_run++; \
    if (!(cond)) { \
        g_tests_failed++; \
        printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    g_tests_run++; \
    if (!((a) == (b))) { \
        g_tests_failed++; \
        printf("FAIL: %s == %s (%s:%d)\n", #a, #b, __FILE__, __LINE__); \
    } \
} while (0)

#define TEST_SUMMARY() do { \
    printf("%d/%d checks passed\n", g_tests_run - g_tests_failed, g_tests_run); \
    return g_tests_failed == 0 ? 0 : 1; \
} while (0)
```

- [ ] **Step 2: Write a smoke test**

```cpp
// tests/native/test_smoke.cpp
#include "test_framework.h"

int main() {
    CHECK(1 == 1);
    CHECK_EQ(2 + 2, 4);
    TEST_SUMMARY();
}
```

- [ ] **Step 3: Write the test runner script**

```bash
#!/usr/bin/env bash
# tests/native/run_tests.sh
set -e
cd "$(dirname "$0")"
FAIL=0
for src in test_*.cpp; do
  bin="${src%.cpp}"
  echo "--- Building $src ---"
  g++ -std=c++17 -I. -I../../firmware/DeskMatrix "$src" -o "/tmp/$bin"
  echo "--- Running $bin ---"
  "/tmp/$bin" || FAIL=1
done
exit $FAIL
```

- [ ] **Step 4: Make it executable and run it**

Run: `chmod +x tests/native/run_tests.sh && tests/native/run_tests.sh`
Expected: `--- Running test_smoke ---` then `2/2 checks passed`, exit code 0

- [ ] **Step 5: Commit**

```bash
git add tests/native/
git commit -m "test: add native test harness for host-testable firmware logic"
```

---

### Task 2: ScreenStateMachine

**Files:**
- Create: `firmware/DeskMatrix/ScreenStateMachine.h`
- Test: `tests/native/test_state_machine.cpp`

**Interfaces:**
- Produces: `enum class ScreenMode { WIFI_SETUP, HOME, INTERRUPT_TAKEOVER, DND, BRB }`, `class ScreenStateMachine` with `mode()`, `wifiConfigured()`, `enterTakeover()`, `exitTakeover()`, `tiltLeft()`, `tiltRight()`, `tiltCenter()`

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_state_machine.cpp
#include "test_framework.h"
#include "ScreenStateMachine.h"

int main() {
    ScreenStateMachine sm;
    CHECK(sm.mode() == ScreenMode::WIFI_SETUP);

    sm.wifiConfigured();
    CHECK(sm.mode() == ScreenMode::HOME);

    sm.enterTakeover();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);

    sm.exitTakeover();
    CHECK(sm.mode() == ScreenMode::HOME);

    // Tilt overrides HOME, and returns to HOME on center
    sm.tiltLeft();
    CHECK(sm.mode() == ScreenMode::DND);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::HOME);

    // Tilt overrides INTERRUPT_TAKEOVER, and restores it (not HOME) on center
    sm.enterTakeover();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);
    sm.tiltRight();
    CHECK(sm.mode() == ScreenMode::BRB);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);

    // Guard: enterTakeover() has no effect outside HOME
    ScreenStateMachine sm2;
    sm2.enterTakeover();
    CHECK(sm2.mode() == ScreenMode::WIFI_SETUP);

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `tests/native/run_tests.sh`
Expected: FAIL — `ScreenStateMachine.h` doesn't exist yet, compile error

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/ScreenStateMachine.h
#pragma once

enum class ScreenMode {
    WIFI_SETUP,
    HOME,
    INTERRUPT_TAKEOVER,
    DND,
    BRB
};

class ScreenStateMachine {
public:
    ScreenStateMachine() : mode_(ScreenMode::WIFI_SETUP), preTiltMode_(ScreenMode::HOME) {}

    ScreenMode mode() const { return mode_; }

    void wifiConfigured() {
        if (mode_ == ScreenMode::WIFI_SETUP) mode_ = ScreenMode::HOME;
    }

    void enterTakeover() {
        if (mode_ == ScreenMode::HOME) mode_ = ScreenMode::INTERRUPT_TAKEOVER;
    }

    void exitTakeover() {
        if (mode_ == ScreenMode::INTERRUPT_TAKEOVER) mode_ = ScreenMode::HOME;
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

Run: `tests/native/run_tests.sh`
Expected: `--- Running test_state_machine ---` then `9/9 checks passed`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/ScreenStateMachine.h tests/native/test_state_machine.cpp
git commit -m "feat: add ScreenStateMachine covering Home/takeover/DND/BRB transitions"
```

---

### Task 3: InterruptQueue

**Files:**
- Create: `firmware/DeskMatrix/InterruptQueue.h`
- Test: `tests/native/test_interrupt_queue.cpp`

**Interfaces:**
- Produces: `class InterruptQueue` with `push(const std::string&)`, `bool popFront(std::string& out)`, `bool empty() const`, `size_t size() const`

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_interrupt_queue.cpp
#include "test_framework.h"
#include "InterruptQueue.h"

int main() {
    InterruptQueue q;
    CHECK(q.empty());

    q.push("scr_jira_full");
    q.push("scr_calendar_full");
    CHECK(!q.empty());
    CHECK_EQ(q.size(), (size_t)2);

    std::string first;
    CHECK(q.popFront(first));
    CHECK_EQ(first, "scr_jira_full");

    std::string second;
    CHECK(q.popFront(second));
    CHECK_EQ(second, "scr_calendar_full");

    CHECK(q.empty());
    std::string none;
    CHECK(!q.popFront(none)); // popping empty queue returns false, doesn't crash

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `tests/native/run_tests.sh`
Expected: FAIL — `InterruptQueue.h` doesn't exist

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/InterruptQueue.h
#pragma once
#include <string>
#include <deque>

class InterruptQueue {
public:
    void push(const std::string& screenId) { queue_.push_back(screenId); }
    bool empty() const { return queue_.empty(); }
    size_t size() const { return queue_.size(); }

    bool popFront(std::string& out) {
        if (queue_.empty()) return false;
        out = queue_.front();
        queue_.pop_front();
        return true;
    }

private:
    std::deque<std::string> queue_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `tests/native/run_tests.sh`
Expected: `--- Running test_interrupt_queue ---` then `6/6 checks passed`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/InterruptQueue.h tests/native/test_interrupt_queue.cpp
git commit -m "feat: add InterruptQueue FIFO for queued screen takeovers"
```

---

### Task 4: ConfigModel (schema + JSON parse/serialize)

**Files:**
- Create: `firmware/DeskMatrix/ConfigModel.h`
- Create: `firmware/DeskMatrix/ConfigModel.cpp`
- Test: `tests/native/test_config_model.cpp`

**Interfaces:**
- Produces: `struct WidgetConfig { id, type, style, color, icon (std::string), x, y, w, h (int), dataSource (std::string) }`, `struct DataSourceConfig { id, type (std::string), pollSec (int), location (std::string) }`, `struct AppConfig { homeWidgets (vector<WidgetConfig>), dataSources (vector<DataSourceConfig>), dndArt, brbArt (std::string) }`, `bool parseConfig(const std::string& json, AppConfig& out, std::string& error)`, `std::string serializeConfig(const AppConfig& config)`

- [ ] **Step 1: Install ArduinoJson for both the ESP32 build and native tests**

Run: `arduino-cli lib install ArduinoJson`

This installs to `~/Documents/Arduino/libraries/ArduinoJson/src` — note this path, it's needed for native compilation since ArduinoJson is header-only and works fine outside Arduino.

- [ ] **Step 2: Write the failing test**

```cpp
// tests/native/test_config_model.cpp
#include "test_framework.h"
#include "ConfigModel.h"

int main() {
    std::string validJson = R"({
        "home": { "widgets": [
            { "id": "w1", "type": "clock", "style": "digital", "color": "#FFDE59", "x": 0, "y": 0, "w": 24, "h": 10 },
            { "id": "w2", "type": "weather", "style": "icon_temp", "icon": "weather_sunny", "color": "#4C9AFF", "x": 0, "y": 40, "w": 16, "h": 16, "dataSource": "ds_weather" }
        ]},
        "dataSources": {
            "ds_weather": { "type": "weather", "pollSec": 600, "location": "40.7128,-74.0060" }
        },
        "dnd": { "art": "dnd_default" },
        "brb": { "art": "brb_default" }
    })";

    AppConfig cfg;
    std::string error;
    CHECK(parseConfig(validJson, cfg, error));
    CHECK_EQ(cfg.homeWidgets.size(), (size_t)2);
    CHECK_EQ(cfg.homeWidgets[0].id, "w1");
    CHECK_EQ(cfg.homeWidgets[0].type, "clock");
    CHECK_EQ(cfg.homeWidgets[1].dataSource, "ds_weather");
    CHECK_EQ(cfg.dataSources.size(), (size_t)1);
    CHECK_EQ(cfg.dataSources[0].id, "ds_weather");
    CHECK_EQ(cfg.dataSources[0].pollSec, 600);
    CHECK_EQ(cfg.dndArt, "dnd_default");

    // Missing required fields rejected
    AppConfig bad;
    std::string badError;
    std::string missingType = R"({"home":{"widgets":[{"id":"w1","x":0,"y":0,"w":1,"h":1}]}})";
    CHECK(!parseConfig(missingType, bad, badError));
    CHECK(!badError.empty());

    // Malformed JSON rejected
    AppConfig bad2;
    std::string badError2;
    CHECK(!parseConfig("{not json", bad2, badError2));

    // Round-trip: parse -> serialize -> parse gives back same widget count/ids
    std::string serialized = serializeConfig(cfg);
    AppConfig reparsed;
    std::string reparseError;
    CHECK(parseConfig(serialized, reparsed, reparseError));
    CHECK_EQ(reparsed.homeWidgets.size(), cfg.homeWidgets.size());
    CHECK_EQ(reparsed.homeWidgets[0].id, cfg.homeWidgets[0].id);
    CHECK_EQ(reparsed.dataSources[0].id, cfg.dataSources[0].id);

    TEST_SUMMARY();
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `tests/native/run_tests.sh`
Expected: FAIL — `ConfigModel.h`/`.cpp` don't exist. Note: from this point on, `run_tests.sh` needs the ArduinoJson include path — update it:

```bash
# tests/native/run_tests.sh (updated)
#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
ARDUINOJSON_SRC="$HOME/Documents/Arduino/libraries/ArduinoJson/src"
FAIL=0
for src in test_*.cpp; do
  bin="${src%.cpp}"
  echo "--- Building $src ---"
  g++ -std=c++17 -I. -I../../firmware/DeskMatrix -I"$ARDUINOJSON_SRC" \
      "$src" ../../firmware/DeskMatrix/ConfigModel.cpp -o "/tmp/$bin"
  echo "--- Running $bin ---"
  "/tmp/$bin" || FAIL=1
done
exit $FAIL
```

- [ ] **Step 4: Write ConfigModel.h**

```cpp
// firmware/DeskMatrix/ConfigModel.h
#pragma once
#include <string>
#include <vector>

struct WidgetConfig {
    std::string id;
    std::string type;
    std::string style;
    std::string color;
    std::string icon;
    int x = 0, y = 0, w = 0, h = 0;
    std::string dataSource;
};

struct DataSourceConfig {
    std::string id;
    std::string type;
    int pollSec = 300;
    std::string location;
};

struct AppConfig {
    std::vector<WidgetConfig> homeWidgets;
    std::vector<DataSourceConfig> dataSources;
    std::string dndArt = "dnd_default";
    std::string brbArt = "brb_default";
};

// Returns true and fills `out` on success; returns false and fills `error` on failure.
// A widget missing `id` or `type` is treated as invalid.
bool parseConfig(const std::string& json, AppConfig& out, std::string& error);

std::string serializeConfig(const AppConfig& config);
```

- [ ] **Step 5: Write ConfigModel.cpp**

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

    out.homeWidgets.clear();
    for (JsonObject w : doc["home"]["widgets"].as<JsonArray>()) {
        WidgetConfig wc;
        wc.id = std::string(w["id"] | "");
        wc.type = std::string(w["type"] | "");
        wc.style = std::string(w["style"] | "");
        wc.color = std::string(w["color"] | "");
        wc.icon = std::string(w["icon"] | "");
        wc.x = w["x"] | 0;
        wc.y = w["y"] | 0;
        wc.w = w["w"] | 0;
        wc.h = w["h"] | 0;
        wc.dataSource = std::string(w["dataSource"] | "");
        if (wc.id.empty() || wc.type.empty()) {
            error = "widget missing required id/type";
            return false;
        }
        out.homeWidgets.push_back(wc);
    }

    out.dataSources.clear();
    JsonObject sources = doc["dataSources"].as<JsonObject>();
    for (JsonPair kv : sources) {
        DataSourceConfig dsc;
        dsc.id = std::string(kv.key().c_str());
        JsonObject v = kv.value().as<JsonObject>();
        dsc.type = std::string(v["type"] | "");
        dsc.pollSec = v["pollSec"] | 300;
        dsc.location = std::string(v["location"] | "");
        out.dataSources.push_back(dsc);
    }

    out.dndArt = std::string(doc["dnd"]["art"] | "dnd_default");
    out.brbArt = std::string(doc["brb"]["art"] | "brb_default");

    return true;
}

std::string serializeConfig(const AppConfig& config) {
    JsonDocument doc;
    JsonArray widgets = doc["home"]["widgets"].to<JsonArray>();
    for (const auto& w : config.homeWidgets) {
        JsonObject wo = widgets.add<JsonObject>();
        wo["id"] = w.id;
        wo["type"] = w.type;
        wo["style"] = w.style;
        wo["color"] = w.color;
        wo["icon"] = w.icon;
        wo["x"] = w.x;
        wo["y"] = w.y;
        wo["w"] = w.w;
        wo["h"] = w.h;
        if (!w.dataSource.empty()) wo["dataSource"] = w.dataSource;
    }

    JsonObject sources = doc["dataSources"].to<JsonObject>();
    for (const auto& ds : config.dataSources) {
        JsonObject so = sources[ds.id].to<JsonObject>();
        so["type"] = ds.type;
        so["pollSec"] = ds.pollSec;
        if (!ds.location.empty()) so["location"] = ds.location;
    }

    doc["dnd"]["art"] = config.dndArt;
    doc["brb"]["art"] = config.brbArt;

    std::string out;
    serializeJson(doc, out);
    return out;
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `chmod +x tests/native/run_tests.sh && tests/native/run_tests.sh`
Expected: `--- Running test_config_model ---` then all checks passed (14/14)

- [ ] **Step 7: Commit**

```bash
git add firmware/DeskMatrix/ConfigModel.h firmware/DeskMatrix/ConfigModel.cpp tests/native/
git commit -m "feat: add ConfigModel with JSON parse/serialize matching the design spec schema"
```

---

### Task 5: WidgetRegistry

**Files:**
- Create: `firmware/DeskMatrix/WidgetRegistry.h`
- Test: `tests/native/test_widget_registry.cpp`

**Interfaces:**
- Consumes: `WidgetConfig` from Task 4 (`ConfigModel.h`)
- Produces: `struct RenderContext { void* displayHandle; const WidgetConfig* widget; }`, `using DrawFn = std::function<void(const RenderContext&)>`, `class WidgetRegistry` with `registerType(type, fn)`, `has(type) const`, `draw(type, ctx) const`

Note on `RenderContext::displayHandle`: it's `void*` (not a concrete display type) specifically so this header — and the registry mechanics tested here — stay compilable and testable without any Arduino/hardware dependency. Real widget draw functions (Task 9+) `static_cast` it back to `MatrixPanel_I2S_DMA*` internally. This is decided now and doesn't change later.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_widget_registry.cpp
#include "test_framework.h"
#include "WidgetRegistry.h"

int main() {
    WidgetRegistry registry;
    CHECK(!registry.has("clock"));

    bool called = false;
    registry.registerType("clock", [&called](const RenderContext&) { called = true; });
    CHECK(registry.has("clock"));

    WidgetConfig w;
    w.id = "w1"; w.type = "clock";
    RenderContext ctx{nullptr, &w};
    CHECK(registry.draw("clock", ctx));
    CHECK(called);

    CHECK(!registry.draw("nonexistent_type", ctx)); // returns false, doesn't crash

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `tests/native/run_tests.sh`
Expected: FAIL — `WidgetRegistry.h` doesn't exist

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/WidgetRegistry.h
#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "ConfigModel.h"

// displayHandle is void* (rather than a concrete display type) so this
// header — and the registry mechanics it defines — stay testable without
// any hardware/Arduino dependency. Real widget draw functions (Task 9+)
// cast it back to the concrete display type internally.
struct RenderContext {
    void* displayHandle = nullptr;
    const WidgetConfig* widget = nullptr;
};

using DrawFn = std::function<void(const RenderContext&)>;

class WidgetRegistry {
public:
    void registerType(const std::string& type, DrawFn fn) { entries_[type] = fn; }
    bool has(const std::string& type) const { return entries_.count(type) > 0; }

    bool draw(const std::string& type, const RenderContext& ctx) const {
        auto it = entries_.find(type);
        if (it == entries_.end()) return false;
        it->second(ctx);
        return true;
    }

private:
    std::unordered_map<std::string, DrawFn> entries_;
};
```

Since this test file needs `ConfigModel.cpp` linked in too (for `WidgetConfig`, even though the registry itself is header-only), update `run_tests.sh`'s build line to always link `ConfigModel.cpp`:

```bash
  g++ -std=c++17 -I. -I../../firmware/DeskMatrix -I"$ARDUINOJSON_SRC" \
      "$src" ../../firmware/DeskMatrix/ConfigModel.cpp -o "/tmp/$bin"
```

(This is already the form from Task 4 — no change needed if you copied it forward.)

- [ ] **Step 4: Run test to verify it passes**

Run: `tests/native/run_tests.sh`
Expected: `--- Running test_widget_registry ---` then `4/4 checks passed`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/WidgetRegistry.h tests/native/test_widget_registry.cpp
git commit -m "feat: add WidgetRegistry mapping widget type strings to draw functions"
```

---

### Task 6: Tilt debounce logic

**Files:**
- Create: `firmware/DeskMatrix/TiltDebouncer.h`
- Test: `tests/native/test_tilt_debouncer.cpp`

**Interfaces:**
- Produces: `enum class TiltDirection { CENTER, LEFT, RIGHT }`, `class TiltDebouncer` with constructor `(float thresholdDeg, int stableReadingsNeeded)` and `TiltDirection update(float angleDeg)`

- [ ] **Step 1: Write the failing test**

```cpp
// tests/native/test_tilt_debouncer.cpp
#include "test_framework.h"
#include "TiltDebouncer.h"

int main() {
    TiltDebouncer d(25.0f, 5);

    // Level readings stay CENTER
    for (int i = 0; i < 10; i++) CHECK(d.update(0.0f) == TiltDirection::CENTER);

    // A single spurious spike doesn't trip it
    CHECK(d.update(-30.0f) == TiltDirection::CENTER);
    CHECK(d.update(0.0f) == TiltDirection::CENTER);

    // 5 consecutive readings past threshold -> LEFT
    TiltDirection last = TiltDirection::CENTER;
    for (int i = 0; i < 5; i++) last = d.update(-30.0f);
    CHECK(last == TiltDirection::LEFT);

    // Returning to level for 5 readings -> CENTER
    for (int i = 0; i < 5; i++) last = d.update(0.0f);
    CHECK(last == TiltDirection::CENTER);

    // Right tilt works the same way
    for (int i = 0; i < 5; i++) last = d.update(30.0f);
    CHECK(last == TiltDirection::RIGHT);

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `tests/native/run_tests.sh`
Expected: FAIL — `TiltDebouncer.h` doesn't exist

- [ ] **Step 3: Write the implementation**

```cpp
// firmware/DeskMatrix/TiltDebouncer.h
#pragma once

enum class TiltDirection { CENTER, LEFT, RIGHT };

// Debounces a stream of tilt-angle readings (degrees, positive = right,
// negative = left) into a stable TiltDirection, so brief bumps don't
// trigger DND/BRB. `thresholdDeg` is how far from level counts as a tilt;
// `stableReadingsNeeded` is how many consecutive readings past threshold
// (or back within it) are required before the reported direction changes.
class TiltDebouncer {
public:
    TiltDebouncer(float thresholdDeg, int stableReadingsNeeded)
        : threshold_(thresholdDeg), needed_(stableReadingsNeeded) {}

    TiltDirection update(float angleDeg) {
        TiltDirection instant = TiltDirection::CENTER;
        if (angleDeg <= -threshold_) instant = TiltDirection::LEFT;
        else if (angleDeg >= threshold_) instant = TiltDirection::RIGHT;

        if (instant == pendingDirection_) {
            pendingCount_++;
        } else {
            pendingDirection_ = instant;
            pendingCount_ = 1;
        }

        if (pendingCount_ >= needed_) {
            stable_ = pendingDirection_;
        }
        return stable_;
    }

private:
    float threshold_;
    int needed_;
    TiltDirection pendingDirection_ = TiltDirection::CENTER;
    int pendingCount_ = 0;
    TiltDirection stable_ = TiltDirection::CENTER;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `tests/native/run_tests.sh`
Expected: `--- Running test_tilt_debouncer ---` then `9/9 checks passed`. Confirm all six native test binaries (`test_smoke`, `test_state_machine`, `test_interrupt_queue`, `test_config_model`, `test_widget_registry`, `test_tilt_debouncer`) pass.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/TiltDebouncer.h tests/native/test_tilt_debouncer.cpp
git commit -m "feat: add TiltDebouncer for stable DND/BRB tilt detection"
```

---

## Part B — On-device firmware

From here on, "tests" mean flashing the real board and verifying behavior — there's no way to meaningfully unit-test panel rendering, Wi-Fi, IMU reads, or the HTTP server off-hardware. Each task ends with an explicit, concrete verification checklist instead of an automated pass/fail.

### Task 7: DeskMatrix scaffold — panel + Wi-Fi + boot IP display

**Files:**
- Create: `firmware/DeskMatrix/config.h`
- Create: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Produces: global `MatrixPanel_I2S_DMA* dma_display`, `void initPanel()`, `void connectWifi()`, `void showIpForSeconds(const String&, int)` — later tasks call into `setup()`/`loop()` here

- [ ] **Step 1: Create config.h**

```cpp
// firmware/DeskMatrix/config.h
#pragma once

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define BOOT_BUTTON_PIN 0
#define WIFI_RESET_HOLD_MS 5000
#define IP_DISPLAY_SECONDS 10

// MVP simplification: fixed UTC offset rather than full timezone database.
// Change to your local offset in seconds (e.g. -18000 for US Eastern).
#define TIMEZONE_OFFSET_SEC 0
```

- [ ] **Step 2: Create DeskMatrix.ino**

```cpp
// firmware/DeskMatrix/DeskMatrix.ino
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFiManager.h>
#include "config.h"

MatrixPanel_I2S_DMA *dma_display = nullptr;
WiFiManager wm;

void initPanel() {
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e = 9;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

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
}

void loop() {
  // Later tasks: state machine tick, HTTP server handling, IMU polling, widget rendering.
}
```

- [ ] **Step 3: Compile**

Run (from `firmware/DeskMatrix/`):
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```
Expected: compiles with no errors (sketch size printed, no `error:` lines)

- [ ] **Step 4: Upload and verify on-device**

Run:
```bash
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn "esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc" .
```

Verify (physically watch the panel + a serial monitor at 115200 baud):
1. Board reconnects to previously-saved Wi-Fi (no `DeskMatrix-Setup` portal needed, since credentials are already saved from earlier testing)
2. Panel shows `IP:` followed by the device's address for ~10 seconds
3. Panel clears afterward
4. Serial log shows `Wi-Fi connected. IP: <address>`

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/config.h firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: scaffold DeskMatrix sketch — panel init, Wi-Fi connect, boot IP display"
```

---

### Task 8: SettingsStore (LittleFS persistence) + config load on boot

**Files:**
- Create: `firmware/DeskMatrix/SettingsStore.h`
- Create: `firmware/DeskMatrix/SettingsStore.cpp`
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `AppConfig`, `parseConfig`, `serializeConfig` from Task 4
- Produces: `class SettingsStore` with `begin()`, `load(std::string& outJson)`, `save(const std::string& json)`

- [ ] **Step 1: Write SettingsStore.h**

```cpp
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
```

- [ ] **Step 2: Write SettingsStore.cpp**

```cpp
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
```

- [ ] **Step 3: Wire into DeskMatrix.ino — load on boot, fall back to a default config**

Add near the top of `DeskMatrix.ino` (after existing includes):
```cpp
#include "ConfigModel.h"
#include "SettingsStore.h"

SettingsStore settingsStore;
AppConfig appConfig;

AppConfig defaultConfig() {
  AppConfig cfg;
  WidgetConfig clock;
  clock.id = "w1"; clock.type = "clock"; clock.style = "digital";
  clock.color = "#FFDE59"; clock.x = 0; clock.y = 0; clock.w = 24; clock.h = 10;
  cfg.homeWidgets.push_back(clock);

  WidgetConfig weather;
  weather.id = "w2"; weather.type = "weather"; weather.style = "icon_temp";
  weather.icon = "weather_sunny"; weather.color = "#4C9AFF";
  weather.x = 0; weather.y = 40; weather.w = 16; weather.h = 16;
  weather.dataSource = "ds_weather";
  cfg.homeWidgets.push_back(weather);

  DataSourceConfig ds;
  ds.id = "ds_weather"; ds.type = "weather"; ds.pollSec = 600;
  ds.location = "40.7128,-74.0060"; // MVP default; overwritten by real config once pushed
  cfg.dataSources.push_back(ds);

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
```

Add to `setup()`, after `connectWifi();`:
```cpp
  settingsStore.begin();
  loadOrInitConfig();
```

- [ ] **Step 4: Compile, upload, verify on-device**

Compile/upload with the same command as Task 7.

Verify via serial monitor:
1. First boot after this change: `No stored config found, using default.`
2. Power-cycle the board (unplug/replug, or press RST): second boot logs `Loaded config from flash.` — confirms the default config was actually persisted and reloaded

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/SettingsStore.h firmware/DeskMatrix/SettingsStore.cpp firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: add SettingsStore (LittleFS) and load/persist config on boot"
```

---

### Task 9: ClockWidget (digital style)

**Files:**
- Create: `firmware/DeskMatrix/screens/ClockWidget.h`
- Create: `firmware/DeskMatrix/screens/ClockWidget.cpp`
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `RenderContext`, `WidgetRegistry` from Task 5
- Produces: `void registerClockWidget(WidgetRegistry& registry)`

- [ ] **Step 1: Write ClockWidget.h**

```cpp
// firmware/DeskMatrix/screens/ClockWidget.h
#pragma once
#include "../WidgetRegistry.h"

void registerClockWidget(WidgetRegistry& registry);
```

- [ ] **Step 2: Write ClockWidget.cpp**

```cpp
// firmware/DeskMatrix/screens/ClockWidget.cpp
#include "ClockWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include <cstdlib>

static void drawDigitalClock(const RenderContext& ctx) {
    auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
    if (!display || !ctx.widget) return;

    time_t now = time(nullptr);
    struct tm timeInfo;
    localtime_r(&now, &timeInfo);

    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);

    uint16_t color = display->color565(255, 222, 89);
    if (ctx.widget->color.size() == 7 && ctx.widget->color[0] == '#') {
        long rgb = strtol(ctx.widget->color.c_str() + 1, nullptr, 16);
        color = display->color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    display->setTextSize(1);
    display->setTextColor(color);
    display->setCursor(ctx.widget->x, ctx.widget->y);
    display->print(buf);
}

void registerClockWidget(WidgetRegistry& registry) {
    registry.registerType("clock", drawDigitalClock);
}
```

- [ ] **Step 3: Wire into DeskMatrix.ino**

Add include near the top:
```cpp
#include "WidgetRegistry.h"
#include "screens/ClockWidget.h"
```

Add global:
```cpp
WidgetRegistry widgetRegistry;
```

Add to `setup()`, after `loadOrInitConfig();`:
```cpp
  registerClockWidget(widgetRegistry);

  configTime(TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org");
```

Replace `loop()` body with:
```cpp
void loop() {
  static unsigned long lastRenderMs = 0;
  unsigned long nowMs = millis();
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    dma_display->clearScreen();
    for (const auto& w : appConfig.homeWidgets) {
      widgetRegistry.draw(w.type, RenderContext{dma_display, &w});
    }
  }
}
```

- [ ] **Step 4: Compile, upload, verify on-device**

Compile/upload with the same command as Task 7.

Verify: panel shows `HH:MM` in the top-left, updating once per minute, matching wall-clock time (accounting for `TIMEZONE_OFFSET_SEC`, which is 0/UTC by default — set it to your local offset in `config.h` if you want local time displayed, recompile/reupload).

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/screens/ClockWidget.h firmware/DeskMatrix/screens/ClockWidget.cpp firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: add digital ClockWidget, wire into Home render loop"
```

---

### Task 10: WeatherService + WeatherWidget

**Files:**
- Create: `firmware/DeskMatrix/services/WeatherService.h`
- Create: `firmware/DeskMatrix/services/WeatherService.cpp`
- Create: `firmware/DeskMatrix/screens/WeatherWidget.h`
- Create: `firmware/DeskMatrix/screens/WeatherWidget.cpp`
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Produces: `struct WeatherReading { temperatureC (float), weatherCode (int), valid (bool) }`, `class WeatherService` with constructor `(float lat, float lon, int pollSec)`, `loop()`, `latest() const`; `void registerWeatherWidget(WidgetRegistry&, WeatherService&)`

Simplification stated explicitly (not a placeholder): this task renders **temperature as text only** — icon rendering is deferred until the asset upload endpoint (Task 12) exists, since there's no bitmap-drawing helper to hook into yet. The `icon` field is still parsed/stored in config from Task 4; it's just unused by the draw function until a later task adds icon support.

- [ ] **Step 1: Write WeatherService.h**

```cpp
// firmware/DeskMatrix/services/WeatherService.h
#pragma once

struct WeatherReading {
    float temperatureC = 0;
    int weatherCode = 0;
    bool valid = false;
};

class WeatherService {
public:
    WeatherService(float latitude, float longitude, int pollSec);
    void loop(); // call every loop() iteration; internally rate-limits to pollSec
    WeatherReading latest() const { return latest_; }

private:
    void fetch();
    float lat_, lon_;
    int pollSec_;
    unsigned long lastFetchMs_ = 0;
    WeatherReading latest_;
};
```

- [ ] **Step 2: Write WeatherService.cpp**

```cpp
// firmware/DeskMatrix/services/WeatherService.cpp
#include "WeatherService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherService::WeatherService(float latitude, float longitude, int pollSec)
    : lat_(latitude), lon_(longitude), pollSec_(pollSec) {}

void WeatherService::loop() {
    unsigned long nowMs = millis();
    if (lastFetchMs_ != 0 && (nowMs - lastFetchMs_) < (unsigned long)pollSec_ * 1000UL) {
        return;
    }
    fetch();
    lastFetchMs_ = nowMs;
}

void WeatherService::fetch() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    char url[192];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code",
        lat_, lon_);
    // Simplification: HTTPClient::begin(url) on esp32 core 3.x auto-creates a
    // secure client for https:// and skips certificate validation. Acceptable
    // for a public, non-sensitive weather API on an MVP; revisit if this
    // pattern gets reused for anything handling sensitive data.
    http.begin(url);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return; // keep last-known-good reading, per the design spec's error handling
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return;

    latest_.temperatureC = doc["current"]["temperature_2m"] | latest_.temperatureC;
    latest_.weatherCode = doc["current"]["weather_code"] | latest_.weatherCode;
    latest_.valid = true;
}
```

- [ ] **Step 3: Write WeatherWidget.h and .cpp**

```cpp
// firmware/DeskMatrix/screens/WeatherWidget.h
#pragma once
#include "../WidgetRegistry.h"
#include "../services/WeatherService.h"

void registerWeatherWidget(WidgetRegistry& registry, WeatherService& weatherService);
```

```cpp
// firmware/DeskMatrix/screens/WeatherWidget.cpp
#include "WeatherWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

void registerWeatherWidget(WidgetRegistry& registry, WeatherService& weatherService) {
    registry.registerType("weather", [&weatherService](const RenderContext& ctx) {
        auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
        if (!display || !ctx.widget) return;

        WeatherReading r = weatherService.latest();
        display->setTextSize(1);
        display->setCursor(ctx.widget->x, ctx.widget->y);
        display->setTextColor(display->color565(76, 154, 255));

        if (!r.valid) {
            display->print("--");
            return;
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", r.temperatureC);
        display->print(buf);
    });
}
```

- [ ] **Step 4: Wire into DeskMatrix.ino**

Add includes:
```cpp
#include "services/WeatherService.h"
#include "screens/WeatherWidget.h"
```

Add helper to parse `"lat,lon"` from config and a global (after `AppConfig appConfig;`):
```cpp
WeatherService* weatherService = nullptr;

void initWeatherService() {
  float lat = 0, lon = 0;
  int pollSec = 600;
  for (const auto& ds : appConfig.dataSources) {
    if (ds.type == "weather") {
      sscanf(ds.location.c_str(), "%f,%f", &lat, &lon);
      pollSec = ds.pollSec;
      break;
    }
  }
  weatherService = new WeatherService(lat, lon, pollSec);
}
```

In `setup()`, after `registerClockWidget(widgetRegistry);`:
```cpp
  initWeatherService();
  registerWeatherWidget(widgetRegistry, *weatherService);
```

In `loop()`, add at the top (before the render-timer block):
```cpp
  weatherService->loop();
```

- [ ] **Step 5: Compile, upload, verify on-device**

Compile/upload with the same command as Task 7.

Verify: within `pollSec` of boot (up to 10 minutes with the default config — temporarily lower `pollSec` in the default config to e.g. 30 for faster testing, then revert), the weather widget shows a temperature (e.g. `18C`) that's plausible for the configured lat/lon. Cross-check against a weather site for the same coordinates.

- [ ] **Step 6: Commit**

```bash
git add firmware/DeskMatrix/services/WeatherService.h firmware/DeskMatrix/services/WeatherService.cpp \
        firmware/DeskMatrix/screens/WeatherWidget.h firmware/DeskMatrix/screens/WeatherWidget.cpp \
        firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: add WeatherService (Open-Meteo) and WeatherWidget, wire into Home"
```

---

### Task 11: IMU wiring + DND/BRB screens

**Files:**
- Create: `firmware/DeskMatrix/services/ImuHardware.h`
- Create: `firmware/DeskMatrix/services/ImuHardware.cpp`
- Create: `firmware/DeskMatrix/screens/DndScreen.h`
- Create: `firmware/DeskMatrix/screens/DndScreen.cpp`
- Create: `firmware/DeskMatrix/screens/BrbScreen.h`
- Create: `firmware/DeskMatrix/screens/BrbScreen.cpp`
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `ScreenStateMachine` (Task 2), `TiltDebouncer`/`TiltDirection` (Task 6)
- Produces: `bool imuBegin()`, `float imuReadTiltDegrees()`, `void drawDndScreen(MatrixPanel_I2S_DMA*)`, `void drawBrbScreen(MatrixPanel_I2S_DMA*)`

- [ ] **Step 1: Find the board's exact IMU init code**

The onboard QMI8658 IMU's I2C pins and init sequence are board-specific and shouldn't be guessed. Waveshare's own example already proves the correct configuration for this exact board:

```bash
# Re-clone if the earlier /tmp copy is gone:
git clone --depth 1 https://github.com/waveshareteam/ESP32-S3-RGB-Matrix.git /tmp/ws-rgb-matrix-repo
```

Read `/tmp/ws-rgb-matrix-repo/example/arduino_v3.3.7/08_Sensor_Test/08_Sensor_Test.ino` and note: which QMI8658 library it uses (install it via `arduino-cli lib install "<name>"` if it's in the Library Manager, or check the repo's `libraries/` folder if it's bundled), which I2C SDA/SCL pins it configures, and its exact init call sequence.

- [ ] **Step 2: Write ImuHardware.h**

```cpp
// firmware/DeskMatrix/services/ImuHardware.h
#pragma once

// Initializes the onboard QMI8658 IMU. Returns false if the sensor isn't
// found/doesn't init — caller should disable DND/BRB for this boot (per the
// design spec's error handling) rather than treat this as fatal.
bool imuBegin();

// Returns the current left/right tilt angle in degrees (positive = right,
// negative = left). Only meaningful if imuBegin() returned true.
float imuReadTiltDegrees();
```

- [ ] **Step 3: Write ImuHardware.cpp using the library/pins/init found in Step 1**

Implement `imuBegin()` calling that library's init function with the pins/address from Waveshare's example. Implement `imuReadTiltDegrees()` by reading the accelerometer X/Y/Z and computing the angle with `atan2()` on whichever two axes correspond to left/right tilt (degrees = `atan2(x, z) * 180.0 / PI` is the typical form, but which axis is "left/right" depends on how the sensor is mounted on this board).

To determine and verify the correct axis/sign: flash this code with a temporary `Serial.println(imuReadTiltDegrees())` in `loop()`, watch the serial monitor while physically tilting the board left and right, and confirm the printed angle goes negative when tilted left and positive when tilted right (per the `TiltDebouncer` contract from Task 6). If the sign or axis is backwards, swap the axes or negate the result — this is a one-line fix once you've observed the actual behavior.

- [ ] **Step 4: Write placeholder-but-functional DND/BRB art**

```cpp
// firmware/DeskMatrix/screens/DndScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
void drawDndScreen(MatrixPanel_I2S_DMA* display);
```

```cpp
// firmware/DeskMatrix/screens/DndScreen.cpp
#include "DndScreen.h"

void drawDndScreen(MatrixPanel_I2S_DMA* display) {
    display->clearScreen();
    display->fillScreen(display->color565(20, 0, 0));
    display->setTextSize(1);
    display->setTextColor(display->color565(255, 60, 60));
    display->setCursor(18, 28);
    display->print("DND");
}
```

```cpp
// firmware/DeskMatrix/screens/BrbScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
void drawBrbScreen(MatrixPanel_I2S_DMA* display);
```

```cpp
// firmware/DeskMatrix/screens/BrbScreen.cpp
#include "BrbScreen.h"

void drawBrbScreen(MatrixPanel_I2S_DMA* display) {
    display->clearScreen();
    display->fillScreen(display->color565(0, 10, 25));
    display->setTextSize(1);
    display->setTextColor(display->color565(80, 180, 255));
    display->setCursor(18, 28);
    display->print("BRB");
}
```

(These are real, working, functional screens — simple by design for MVP. Swapping in the nicer artistic bitmaps designed earlier is a later, purely additive change: same function signature, different drawing calls.)

- [ ] **Step 5: Wire into DeskMatrix.ino**

Add includes:
```cpp
#include "ScreenStateMachine.h"
#include "TiltDebouncer.h"
#include "services/ImuHardware.h"
#include "screens/DndScreen.h"
#include "screens/BrbScreen.h"
```

Add globals (after `WeatherService* weatherService = nullptr;`):
```cpp
ScreenStateMachine stateMachine;
TiltDebouncer tiltDebouncer(25.0f, 5);
bool imuAvailable = false;
```

In `setup()`, after `registerWeatherWidget(...)`:
```cpp
  imuAvailable = imuBegin();
  if (!imuAvailable) Serial.println("IMU not found — DND/BRB disabled this boot.");

  stateMachine.wifiConfigured(); // Wi-Fi already connected above; move state machine to HOME
```

Replace `loop()` with:
```cpp
void loop() {
  weatherService->loop();

  unsigned long nowMs = millis();

  static unsigned long lastImuPollMs = 0;
  if (imuAvailable && (nowMs - lastImuPollMs) >= 150) {
    lastImuPollMs = nowMs;
    float angle = imuReadTiltDegrees();
    TiltDirection dir = tiltDebouncer.update(angle);
    if (dir == TiltDirection::LEFT) stateMachine.tiltLeft();
    else if (dir == TiltDirection::RIGHT) stateMachine.tiltRight();
    else stateMachine.tiltCenter();
  }

  static unsigned long lastRenderMs = 0;
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    switch (stateMachine.mode()) {
      case ScreenMode::HOME:
        dma_display->clearScreen();
        for (const auto& w : appConfig.homeWidgets) {
          widgetRegistry.draw(w.type, RenderContext{dma_display, &w});
        }
        break;
      case ScreenMode::DND:
        drawDndScreen(dma_display);
        break;
      case ScreenMode::BRB:
        drawBrbScreen(dma_display);
        break;
      case ScreenMode::INTERRUPT_TAKEOVER:
        // Scaffolded for future data sources (e.g. Jira) — MVP's `screens`
        // list is empty, so this state is never entered yet.
        break;
      default:
        break;
    }
  }
}
```

- [ ] **Step 6: Compile, upload, verify on-device**

Compile/upload with the same command as Task 7.

Verify (physically):
1. Panel shows Home (Clock + Weather) at rest
2. Tilt the enclosure left and hold — within ~1 second, panel switches to the red "DND" screen
3. Return to level — within ~1 second, panel returns to Home
4. Tilt right and hold — panel switches to the blue "BRB" screen; return to level — back to Home

- [ ] **Step 7: Commit**

```bash
git add firmware/DeskMatrix/services/ImuHardware.h firmware/DeskMatrix/services/ImuHardware.cpp \
        firmware/DeskMatrix/screens/DndScreen.h firmware/DeskMatrix/screens/DndScreen.cpp \
        firmware/DeskMatrix/screens/BrbScreen.h firmware/DeskMatrix/screens/BrbScreen.cpp \
        firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: wire IMU tilt detection to DND/BRB screens via ScreenStateMachine"
```

---

### Task 12: ConfigServer HTTP API

**Files:**
- Create: `firmware/DeskMatrix/web/ConfigServer.h`
- Create: `firmware/DeskMatrix/web/ConfigServer.cpp`
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Consumes: `AppConfig`, `parseConfig`, `serializeConfig` (Task 4), `SettingsStore` (Task 8)
- Produces: `class ConfigServer` with constructor `(AppConfig&, SettingsStore&)`, `begin()`, `loop()`

- [ ] **Step 1: Write ConfigServer.h**

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

private:
    void handleGetConfig();
    void handlePutConfig();
    void handlePostAsset();
    void handleOtaUpload();

    WebServer server_;
    AppConfig& appConfig_;
    SettingsStore& store_;
};
```

- [ ] **Step 2: Write ConfigServer.cpp**

```cpp
// firmware/DeskMatrix/web/ConfigServer.cpp
#include "ConfigServer.h"
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
```

- [ ] **Step 3: Wire into DeskMatrix.ino**

Add include:
```cpp
#include "web/ConfigServer.h"
```

Add global (after `SettingsStore settingsStore;` / `AppConfig appConfig;`):
```cpp
ConfigServer configServer(appConfig, settingsStore);
```

In `setup()`, after `stateMachine.wifiConfigured();`:
```cpp
  configServer.begin();
  Serial.print("Config API ready at http://");
  Serial.println(WiFi.localIP());
```

In `loop()`, add as the very first line:
```cpp
  configServer.loop();
```

- [ ] **Step 4: Compile, upload, verify via curl from the Mac**

Compile/upload with the same command as Task 7. Note the device's IP from the serial log, then (replacing `<device-ip>`):

```bash
curl http://<device-ip>/api/config
```
Expected: JSON matching current config (Clock + Weather widgets)

```bash
curl -X PUT http://<device-ip>/api/config \
  -H "Content-Type: application/json" \
  -d '{"home":{"widgets":[{"id":"w1","type":"clock","style":"digital","color":"#FF0000","x":10,"y":10,"w":24,"h":10}]},"dataSources":{},"dnd":{"art":"dnd_default"},"brb":{"art":"brb_default"}}'
```
Expected: `{"status":"ok"}`, and the panel's clock immediately moves to position (10,10) in red — confirms live config application

```bash
curl http://<device-ip>/api/config
```
Expected: reflects the new widget position — confirms it's not just applied live but also readable back

Power-cycle the board, then:
```bash
curl http://<device-ip>/api/config
```
Expected: same updated config (position 10,10) — confirms it persisted to LittleFS and survived reboot

```bash
curl -X PUT http://<device-ip>/api/config -d '{not valid json'
```
Expected: HTTP 400 with an `{"error": ...}` body; a follow-up `GET /api/config` still shows the last-valid config, not corrupted

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/web/ConfigServer.h firmware/DeskMatrix/web/ConfigServer.cpp firmware/DeskMatrix/DeskMatrix.ino
git commit -m "feat: add ConfigServer HTTP API (GET/PUT config, asset upload, OTA)"
```

---

### Task 13: End-to-end verification

**Files:** none (verification only)

- [ ] **Step 1: Full checklist on the real device**

1. Cold boot (power-cycle) → panel shows IP for ~10s → Home shows Clock + Weather
2. Tilt left → DND appears within ~1s; return to level → Home resumes with correct widgets
3. Tilt right → BRB appears; return to level → Home resumes
4. `curl GET /api/config` → matches what's on the panel
5. `curl PUT /api/config` with a new widget layout → Home updates within ~1s
6. Power-cycle → new layout persists (LittleFS save/load round-trip confirmed under real reboot, not just the earlier isolated test)
7. `curl PUT /api/config` with malformed JSON → 400 returned, Home unaffected, `GET /api/config` still shows the last-valid config
8. Confirm all six native test binaries still pass: `tests/native/run_tests.sh`

- [ ] **Step 2: Commit final state**

```bash
git add -A
git commit -m "chore: MVP platform complete — Home dashboard, DND/BRB, config API verified end-to-end"
```
