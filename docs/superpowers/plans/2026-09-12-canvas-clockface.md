# Canvas Clockface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a JSON-driven "Canvas" clockface engine to DeskMatrix's idle clock screen, exposed as a paste-your-own-theme config slot plus two bundled presets (Nyan Cat, Star Wars).

**Architecture:** One new `CanvasClockface` class (implements the existing `IClockface` interface, same as `MarioClockface`/`WordsClockface`/`PacmanClockface`) interprets a JSON theme string at construction time into a small vector of prepared draw commands and pre-decoded sprite pixel buffers, then replays them every `update()` call — never re-parsing JSON or re-decoding PNGs in the render hot path. `ClockScreen.cpp`'s `loadClockFace()` feeds it either the user's pasted `AppConfig.canvasJson` (for `"canvas"`) or one of two firmware-embedded theme JSON strings (for `"nyancat"`/`"starwars"`).

**Tech Stack:** ArduinoJson 7.4.3 (already a project dependency), PNGdec 1.1.6 (bitbank2 — new dependency, same author/API shape as the project's existing `AnimatedGIF`), mbedtls's `base64.h` (already linked in via the ESP32 Arduino core's TLS support), Adafruit_GFX primitives on the project's existing `GFXcanvas16`.

## Global Constraints

- `canvasJson` is capped at 64KB in `parseConfig()` — comfortably above the largest known real clock-club theme (Christmas Snoopy, ~13.5KB).
- Decoded PNG source bytes (post-base64) are capped at 8192 bytes per image — no legitimate clock-sized sprite needs more; guards against a malformed/huge pasted value.
- Decoded sprite pixel buffers are allocated via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` — keep internal RAM free for WiFi/TLS/JSON-parse scratch space, per the spec's stated budget approach.
- A malformed individual element (bad base64, undecodable PNG, unrecognized `type`) is skipped, never crashes the clockface. A theme that fails to parse entirely falls back to Mario.
- All new root-level `.cpp` files go directly in `firmware/DeskMatrix/` (not subdirectories) — arduino-cli only auto-compiles sketch-root `.cpp` files; this is an established, repeatedly-confirmed constraint of this codebase's build.
- Font/preset asset files are vendored **verbatim** from their exact upstream URLs (given below) — never hand-typed or paraphrased, matching how the Mario/Words/Pacman clockfaces were ported.

---

### Task 1: `CWDateTime::getYear()` + ezTime format translator

**Files:**
- Modify: `firmware/DeskMatrix/lib/cw-commons/CWDateTime.h`
- Modify: `firmware/DeskMatrix/CWDateTime.cpp`
- Create: `firmware/DeskMatrix/EzTimeFormat.h`
- Create: `firmware/DeskMatrix/EzTimeFormat.cpp`
- Test: `tests/native/test_ez_time_format.cpp`
- Modify: `tests/native/run_tests.sh`

**Interfaces:**
- Produces: `int CWDateTime::getYear()` (returns 4-digit year, e.g. `2026`).
- Produces: `std::string formatEzTime(const std::string& format, int hour24, int minute, int second, int day, int month, int year)` — translates the ezTime format tokens used by real Canvas theme JSON (`H`/`G`/`h`/`g`/`i`/`s`/`d`/`j`/`m`/`n`/`Y`/`y`) into a formatted string; any other character (separators like `:`/`-`) passes through unchanged. Used by Task 4's `CanvasClockface`.

- [ ] **Step 1: Add the failing native test**

Create `tests/native/test_ez_time_format.cpp`:

```cpp
// tests/native/test_ez_time_format.cpp
#include "test_framework.h"
#include "EzTimeFormat.h"

int main() {
    // "H:i" -- the format every real Canvas theme uses for its clock digits
    CHECK_EQ(formatEzTime("H:i", 9, 5, 30, 12, 3, 2026), "09:05");
    CHECK_EQ(formatEzTime("H:i", 23, 59, 0, 1, 1, 2026), "23:59");

    // "m-d" -- the date-line format seen in the real Nyan Cat theme
    CHECK_EQ(formatEzTime("m-d", 9, 5, 30, 3, 12, 2026), "12-03");

    // 12-hour tokens
    CHECK_EQ(formatEzTime("h:i", 0, 5, 0, 1, 1, 2026), "12:05");   // midnight -> 12
    CHECK_EQ(formatEzTime("h:i", 13, 5, 0, 1, 1, 2026), "01:05");  // 1pm -> 01, padded
    CHECK_EQ(formatEzTime("g:i", 13, 5, 0, 1, 1, 2026), "1:05");   // 1pm -> 1, unpadded

    // Unpadded day/month
    CHECK_EQ(formatEzTime("j/n", 9, 5, 30, 3, 12, 2026), "3/12");

    // Year tokens
    CHECK_EQ(formatEzTime("Y", 9, 5, 30, 3, 12, 2026), "2026");
    CHECK_EQ(formatEzTime("y", 9, 5, 30, 3, 12, 2026), "26");

    // Unknown characters (separators) pass through unchanged
    CHECK_EQ(formatEzTime("H:i:s", 9, 5, 30, 3, 12, 2026), "09:05:30");

    TEST_SUMMARY();
}
```

- [ ] **Step 2: Update `run_tests.sh` to link the new source file, then run to confirm it fails**

Edit `tests/native/run_tests.sh` — change the g++ line so every test also links `EzTimeFormat.cpp` (harmless for tests that don't use it, same blanket-link pattern the script already uses for `ConfigModel.cpp`):

```bash
  g++ -std=c++17 -I. -I../../firmware/DeskMatrix -I"$ARDUINOJSON_SRC" -nostdinc++ -isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1 -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
      "$src" ../../firmware/DeskMatrix/ConfigModel.cpp ../../firmware/DeskMatrix/EzTimeFormat.cpp -o "/tmp/$bin"
```

Run: `bash tests/native/run_tests.sh`
Expected: FAIL to build `test_ez_time_format.cpp` — `EzTimeFormat.h: No such file or directory`.

- [ ] **Step 3: Implement `EzTimeFormat`**

Create `firmware/DeskMatrix/EzTimeFormat.h`:

```cpp
// firmware/DeskMatrix/EzTimeFormat.h
#pragma once
#include <string>

// Translates a subset of ezTime's date/time format tokens (used by
// Clockwise's Canvas clockface JSON themes, e.g. "H:i") into a formatted
// string, given already-computed calendar fields. Needed because
// CWDateTime.cpp (see its port-history comment) replaced ezTime with
// strftime-based formatting, which doesn't understand these tokens.
//
// Supported tokens:
//   H/G  24-hour, padded/unpadded
//   h/g  12-hour, padded/unpadded (0 -> 12)
//   i    minute, padded
//   s    second, padded
//   d/j  day of month, padded/unpadded
//   m/n  month, padded/unpadded
//   Y/y  4-digit/2-digit year
// Any other character (e.g. ':', '-') is copied through unchanged.
std::string formatEzTime(const std::string& format, int hour24, int minute,
                          int second, int day, int month, int year);
```

Create `firmware/DeskMatrix/EzTimeFormat.cpp`:

```cpp
// firmware/DeskMatrix/EzTimeFormat.cpp
#include "EzTimeFormat.h"
#include <cstdio>

namespace {
std::string pad2(int v) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", v);
    return std::string(buf);
}
}  // namespace

std::string formatEzTime(const std::string& format, int hour24, int minute,
                          int second, int day, int month, int year) {
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;

    std::string out;
    out.reserve(format.size());
    for (char c : format) {
        switch (c) {
            case 'H': out += pad2(hour24); break;
            case 'G': out += std::to_string(hour24); break;
            case 'h': out += pad2(hour12); break;
            case 'g': out += std::to_string(hour12); break;
            case 'i': out += pad2(minute); break;
            case 's': out += pad2(second); break;
            case 'd': out += pad2(day); break;
            case 'j': out += std::to_string(day); break;
            case 'm': out += pad2(month); break;
            case 'n': out += std::to_string(month); break;
            case 'Y': out += std::to_string(year); break;
            case 'y': out += pad2(year % 100); break;
            default: out += c; break;
        }
    }
    return out;
}
```

- [ ] **Step 4: Run the test to confirm it passes**

Run: `bash tests/native/run_tests.sh`
Expected: `test_ez_time_format` shows `12/12 checks passed`; all other suites still pass.

- [ ] **Step 5: Add `CWDateTime::getYear()`**

Edit `firmware/DeskMatrix/lib/cw-commons/CWDateTime.h` — add to the public section, alongside the other int getters:

```cpp
  int getYear();       // e.g. 2026
```

Edit `firmware/DeskMatrix/CWDateTime.cpp` — add after `getMonth()`:

```cpp
int CWDateTime::getYear()
{
  return getLocalTm().tm_year + 1900;
}
```

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `bash tests/native/run_tests.sh`
Expected: all 9 suites pass (the 8 existing plus `test_ez_time_format`).

- [ ] **Step 7: Commit**

```bash
git add tests/native/test_ez_time_format.cpp tests/native/run_tests.sh \
        firmware/DeskMatrix/EzTimeFormat.h firmware/DeskMatrix/EzTimeFormat.cpp \
        firmware/DeskMatrix/lib/cw-commons/CWDateTime.h firmware/DeskMatrix/CWDateTime.cpp
git commit -m "add ezTime format translator and CWDateTime::getYear() for Canvas clockface"
```

---

### Task 2: `AppConfig.canvasJson` field with validation

**Files:**
- Modify: `firmware/DeskMatrix/ConfigModel.h`
- Modify: `firmware/DeskMatrix/ConfigModel.cpp`
- Test: `tests/native/test_config_model.cpp`

**Interfaces:**
- Produces: `AppConfig.canvasJson` (`std::string`, default `""`) — the user's pasted custom Canvas theme JSON, read by Task 5's `loadClockFace("canvas")`.
- Consumes: nothing from earlier tasks.

- [ ] **Step 1: Add failing tests to `test_config_model.cpp`**

Edit `tests/native/test_config_model.cpp` — insert before the final `TEST_SUMMARY();`:

```cpp
    // canvasJson: absent defaults to empty, no error
    AppConfig noCanvas;
    std::string noCanvasErr;
    CHECK(parseConfig(R"({})", noCanvas, noCanvasErr));
    CHECK_EQ(noCanvas.canvasJson, "");

    // canvasJson: valid theme JSON round-trips through parse -> serialize -> parse
    AppConfig withCanvas;
    withCanvas.canvasJson = R"({"setup":[{"type":"rect","x":0,"y":0,"width":10,"height":10,"color":1}]})";
    std::string withCanvasSerialized = serializeConfig(withCanvas);
    AppConfig reparsedCanvas;
    std::string reparsedCanvasErr;
    CHECK(parseConfig(withCanvasSerialized, reparsedCanvas, reparsedCanvasErr));
    CHECK_EQ(reparsedCanvas.canvasJson, withCanvas.canvasJson);

    // canvasJson: over 64KB is rejected
    AppConfig oversizedCanvas;
    oversizedCanvas.canvasJson = std::string(70000, 'x');
    std::string oversizedSerialized = serializeConfig(oversizedCanvas);
    AppConfig rejectedCanvas;
    std::string rejectedCanvasErr;
    CHECK(!parseConfig(oversizedSerialized, rejectedCanvas, rejectedCanvasErr));
    CHECK(!rejectedCanvasErr.empty());

    // canvasJson: valid JSON but missing setup/loop arrays is rejected
    AppConfig badStructureCfg;
    badStructureCfg.canvasJson = R"({"name":"missing setup and loop"})";
    std::string badStructureSerialized = serializeConfig(badStructureCfg);
    AppConfig rejectedStructure;
    std::string rejectedStructureErr;
    CHECK(!parseConfig(badStructureSerialized, rejectedStructure, rejectedStructureErr));
    CHECK(!rejectedStructureErr.empty());
```

- [ ] **Step 2: Run to confirm it fails**

Run: `bash tests/native/run_tests.sh`
Expected: `test_config_model` fails to build — `AppConfig` has no member `canvasJson`.

- [ ] **Step 3: Add the field and validation**

Edit `firmware/DeskMatrix/ConfigModel.h` — add to `AppConfig`, after `timezoneOffsetMinutes`:

```cpp
    std::string canvasJson; // user-pasted Canvas clockface theme JSON (see screens/clockfaces/canvas/); empty = none configured
```

Edit `firmware/DeskMatrix/ConfigModel.cpp` — add a validation helper above `parseConfig`:

```cpp
namespace {
// Empty is always valid (no custom theme configured yet). Otherwise: size
// capped at 64KB (comfortably above the largest known real clock-club
// theme, ~13.5KB), must be valid JSON, and must have a 'setup' or 'loop'
// array -- catches an empty-object paste or a copy-paste of the wrong
// thing without needing a full CanvasClockface parse here.
bool isValidCanvasJson(const std::string& json, std::string& error) {
    if (json.empty()) return true;
    if (json.size() > 65536) {
        error = "canvasJson exceeds 64KB limit";
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        error = "canvasJson is not valid JSON";
        return false;
    }
    if (!doc["setup"].is<JsonArray>() && !doc["loop"].is<JsonArray>()) {
        error = "canvasJson must have a 'setup' or 'loop' array";
        return false;
    }
    return true;
}
}  // namespace
```

Then in `parseConfig`, add after the `timezoneOffsetMinutes` block:

```cpp
    std::string canvasJson = std::string(doc["canvasJson"] | "");
    std::string canvasError;
    if (!isValidCanvasJson(canvasJson, canvasError)) {
        error = canvasError;
        return false;
    }
    out.canvasJson = canvasJson;
```

And in `serializeConfig`, add after `timezoneOffsetMinutes`:

```cpp
    doc["canvasJson"] = config.canvasJson;
```

- [ ] **Step 4: Run to confirm it passes**

Run: `bash tests/native/run_tests.sh`
Expected: all suites pass, `test_config_model` now shows more checks passed than before.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/ConfigModel.h firmware/DeskMatrix/ConfigModel.cpp tests/native/test_config_model.cpp
git commit -m "add AppConfig.canvasJson with size/structure validation"
```

---

### Task 3: Vendor PNGdec, fonts, and the two bundled preset themes

**Files:**
- Create: `firmware/DeskMatrix/screens/clockfaces/canvas/fonts/atari.h`
- Create: `firmware/DeskMatrix/screens/clockfaces/canvas/fonts/hour8pt7b.h`
- Create: `firmware/DeskMatrix/screens/clockfaces/canvas/fonts/minute7pt7b.h`
- Create: `firmware/DeskMatrix/screens/clockfaces/canvas/presets/nyancat.json.h`
- Create: `firmware/DeskMatrix/screens/clockfaces/canvas/presets/starwars.json.h`

**Interfaces:**
- Produces: `GFXfont atariFont`, `GFXfont hour8pt7b`, `GFXfont minute7pt7b` (global objects, from the vendored font headers) — used by Task 4's `CanvasClockface::setFontByName()`.
- Produces: `static const char kNyanCatJson[]`, `static const char kStarWarsJson[]` (embedded theme JSON strings) — used by Task 5's `loadClockFace()`.
- Consumes: nothing from earlier tasks.

This task is pure asset-vendoring (verbatim copies, like the Mario/Words/Pacman font and sprite assets already in this repo) — nothing here is hand-written or paraphrased, and none of it is natively testable (Arduino/ESP32-only). Verified in Task 7's hardware flash.

- [ ] **Step 1: Install the PNGdec library**

Run: `arduino-cli lib install PNGdec`
Expected: installs PNGdec 1.1.6 (or newer) alongside the project's existing `AnimatedGIF`/`ArduinoJson` libraries. Confirm with `arduino-cli lib list | grep -i pngdec`.

- [ ] **Step 2: Vendor the three font files verbatim**

These are Adafruit-GFX-format `fontconvert` output from Clockwise's own Canvas clockface (`jnthas/cw-cf-0x07`) — the exact same fonts real clock-club themes reference by name (`"square"`, `"big"`, `"medium"`). Download them byte-for-byte; do not hand-edit.

Run:
```bash
mkdir -p firmware/DeskMatrix/screens/clockfaces/canvas/fonts
python3 -c "
import urllib.request
files = {
    'atari.h': 'https://raw.githubusercontent.com/jnthas/cw-cf-0x07/main/fonts/atari.h',
    'hour8pt7b.h': 'https://raw.githubusercontent.com/jnthas/cw-cf-0x07/main/fonts/hour8pt7b.h',
    'minute7pt7b.h': 'https://raw.githubusercontent.com/jnthas/cw-cf-0x07/main/fonts/minute7pt7b.h',
}
for name, url in files.items():
    with urllib.request.urlopen(url) as resp:
        data = resp.read()
    path = 'firmware/DeskMatrix/screens/clockfaces/canvas/fonts/' + name
    with open(path, 'wb') as f:
        f.write(data)
    print(path, len(data), 'bytes')
"
```

Expected output: `atari.h 6126 bytes`, `hour8pt7b.h 11935 bytes`, `minute7pt7b.h` (some similar size). Confirm each file's last non-blank line declares its font object:
```bash
grep -h "^const GFXfont" firmware/DeskMatrix/screens/clockfaces/canvas/fonts/*.h
```
Expected: three lines, declaring `atariFont`, `hour8pt7b`, and `minute7pt7b` respectively.

- [ ] **Step 3: Generate the two bundled preset theme headers**

These embed the real, unmodified theme JSON from `jnthas/clock-club`'s `shared/` folder as C++ string constants. Use this generator script (validates the JSON parses before embedding, so a network hiccup mid-download fails loudly instead of baking in a truncated file):

```bash
mkdir -p firmware/DeskMatrix/screens/clockfaces/canvas/presets
cat > /tmp/gen_canvas_preset.py << 'PYEOF'
#!/usr/bin/env python3
import sys, json

def main():
    in_path, out_path, var_name = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(in_path, "r", encoding="utf-8") as f:
        text = f.read()
    json.loads(text)  # validate before embedding

    guard = var_name.upper() + "_H_INCLUDED"
    lines = [
        "// Auto-generated by gen_canvas_preset.py from %s. Do not hand-edit." % in_path,
        "#pragma once",
        "#ifndef %s" % guard,
        "#define %s" % guard,
        "",
        "static const char %s[] = R\"CANVASJSON(" % var_name,
        text.rstrip("\n"),
        ")CANVASJSON\";",
        "",
        "#endif  // %s" % guard,
        "",
    ]
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("wrote", out_path, len(text), "bytes of JSON embedded")

if __name__ == "__main__":
    main()
PYEOF

python3 -c "
import urllib.request
urls = {
    'nyan-cat.json': 'https://raw.githubusercontent.com/jnthas/clock-club/main/shared/nyan-cat.json',
    'star-wars.json': 'https://raw.githubusercontent.com/jnthas/clock-club/main/shared/star-wars.json',
}
for name, url in urls.items():
    with urllib.request.urlopen(url) as resp:
        data = resp.read()
    with open('/tmp/' + name, 'wb') as f:
        f.write(data)
    print(name, len(data), 'bytes')
"

python3 /tmp/gen_canvas_preset.py /tmp/nyan-cat.json \
    firmware/DeskMatrix/screens/clockfaces/canvas/presets/nyancat.json.h kNyanCatJson
python3 /tmp/gen_canvas_preset.py /tmp/star-wars.json \
    firmware/DeskMatrix/screens/clockfaces/canvas/presets/starwars.json.h kStarWarsJson
```

Expected output: `nyan-cat.json 6957 bytes`, `star-wars.json 2111 bytes`, then `wrote .../nyancat.json.h 6957 bytes of JSON embedded` and `wrote .../starwars.json.h 2111 bytes of JSON embedded`.

Verify no raw-string-terminator collision (the JSON itself must not contain the literal text `)CANVASJSON`, which would prematurely end the C++ raw string):
```bash
grep -c ')CANVASJSON' /tmp/nyan-cat.json /tmp/star-wars.json
```
Expected: `0` for both files (if either shows a nonzero count, pick a different raw-string delimiter, e.g. `R"CANVASJSON2(...)CANVASJSON2"`, and regenerate).

- [ ] **Step 4: Commit**

```bash
git add firmware/DeskMatrix/screens/clockfaces/canvas/fonts firmware/DeskMatrix/screens/clockfaces/canvas/presets
git commit -m "vendor PNGdec-era Canvas fonts and Nyan Cat / Star Wars preset themes"
```

---

### Task 4: `CanvasClockface` interpreter

**Files:**
- Create: `firmware/DeskMatrix/screens/clockfaces/canvas/CanvasClockface.h`
- Create: `firmware/DeskMatrix/CanvasClockface.cpp` (sketch root — see Global Constraints)

**Interfaces:**
- Consumes: `IClockface` (from `firmware/DeskMatrix/lib/cw-commons/IClockface.h`), `CWDateTime` (same directory, plus Task 1's `getYear()`), `formatEzTime()` (Task 1), `atariFont`/`hour8pt7b`/`minute7pt7b` (Task 3).
- Produces: `class CanvasClockface : public IClockface` with constructor `CanvasClockface(Adafruit_GFX* display, const char* themeJson)` and `bool isValid() const` — used by Task 5's `loadClockFace()`.

This class is Arduino/ESP32/PNGdec-dependent and has no native test coverage (same as `MarioClockface`/`WordsClockface`/`PacmanClockface`) — it's written as a complete unit and verified by compiling + flashing in Task 7, not TDD'd step by step.

- [ ] **Step 1: Create the header**

Create `firmware/DeskMatrix/screens/clockfaces/canvas/CanvasClockface.h`:

```cpp
// firmware/DeskMatrix/screens/clockfaces/canvas/CanvasClockface.h
#pragma once
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <string>
#include <vector>

#include "lib/cw-commons/IClockface.h"
#include "lib/cw-commons/CWDateTime.h"
#include "fonts/atari.h"
#include "fonts/hour8pt7b.h"
#include "fonts/minute7pt7b.h"
#include <Fonts/Picopixel.h>

// Interprets a Clockwise "Canvas" (cw-cf-0x07) theme JSON string at
// construction time -- see docs/superpowers/specs/2026-09-12-canvas-clockface-design.md
// for the JSON schema and rationale. Draws setup[] elements once in
// setup(), then redraws loop[] sprites (advancing multi-frame animation)
// and refreshes datetime elements every update().
class CanvasClockface : public IClockface {
  public:
    // themeJson is only read during construction (not retained) -- caller
    // doesn't need to keep it alive afterward. isValid() is false if
    // themeJson failed to parse as JSON; the caller (ClockScreen.cpp) is
    // responsible for falling back to another clockface in that case.
    CanvasClockface(Adafruit_GFX* display, const char* themeJson);
    ~CanvasClockface() override;

    void setup(CWDateTime* dateTime) override;
    void update() override;

    bool isValid() const { return valid_; }

  private:
    struct SetupElement {
        enum Type { RECT, FILLRECT, LINE, TEXT, DATETIME, IMAGE } type = RECT;
        int16_t x = 0, y = 0, x1 = 0, y1 = 0, width = 0, height = 0;
        uint16_t color = 0, fgColor = 0xFFFF, bgColor = 0;
        std::string content;  // text content, or datetime format, or base64 PNG for image
        std::string font;
    };
    struct SpriteFrame {
        uint16_t* pixels = nullptr;  // owned, heap_caps_malloc'd (PSRAM); freed in destructor
        uint16_t width = 0;
        uint16_t height = 0;
    };
    struct LoopSprite {
        int spriteIndex = -1;  // index into loadedSprites_
        int16_t x = 0, y = 0;
        size_t currentFrame = 0;
    };

    void renderShape(const SetupElement& el);
    void renderText(const std::string& text, const SetupElement& el);
    void renderImageElement(const SetupElement& el);
    void renderDatetimeElements();
    void setFontByName(const std::string& name);
    bool loadSpriteFrame(const std::string& base64, SpriteFrame& outFrame);

    Adafruit_GFX* display_ = nullptr;
    CWDateTime* dateTime_ = nullptr;
    bool valid_ = false;
    uint16_t bgColor_ = 0;
    uint16_t delayMs_ = 300;
    unsigned long lastLoopMs_ = 0;
    unsigned long lastDateTimeMs_ = 0;

    std::vector<SetupElement> setupElements_;
    std::vector<std::vector<SpriteFrame>> loadedSprites_;  // [spriteIndex][frameIndex]
    std::vector<LoopSprite> loopSprites_;
};
```

- [ ] **Step 2: Create the implementation**

Create `firmware/DeskMatrix/CanvasClockface.cpp`:

```cpp
// firmware/DeskMatrix/CanvasClockface.cpp
// Lives at the sketch root, like ClockScreen.cpp/MarioBlock.cpp/etc. --
// arduino-cli only auto-compiles root-level .cpp files.
#include "screens/clockfaces/canvas/CanvasClockface.h"

#include <PNGdec.h>
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include <cstring>

#include "EzTimeFormat.h"

namespace {
// Shared PNGdec decode state. Only one CanvasClockface is ever being
// constructed at a time (main/loop task), so a single static decoder
// instance is safe -- same approach Clockwise's own upstream Canvas
// clockface uses (jnthas/cw-cf-0x07's PNGRender.h).
PNG g_png;
bool g_pngDrawDirect = false;
Adafruit_GFX* g_pngDrawTarget = nullptr;
int16_t g_pngDrawX = 0;
int16_t g_pngDrawY = 0;
uint8_t* g_pngDecodeDst = nullptr;
uint16_t g_pngDecodeDstW = 0;
uint16_t g_pngDecodeDstH = 0;

int pngDrawCallback(PNGDRAW* pDraw) {
    uint16_t line[64];
    g_png.getLineAsRGB565(pDraw, line, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    if (g_pngDrawDirect) {
        if (g_pngDrawTarget != nullptr) {
            g_pngDrawTarget->drawRGBBitmap(g_pngDrawX, g_pngDrawY + pDraw->y, line, pDraw->iWidth, 1);
        }
    } else if (g_pngDecodeDst != nullptr && pDraw->y < g_pngDecodeDstH) {
        memcpy(g_pngDecodeDst + (size_t)pDraw->y * g_pngDecodeDstW * 2, line, (size_t)pDraw->iWidth * 2);
    }
    return 1;
}

// Base64-decodes `base64` into `outRaw`. Returns false (leaving outRaw
// untouched) on failure or if the decoded size is implausible for a
// clock-sized sprite -- guards against a malformed/huge pasted value.
bool base64DecodeGuarded(const std::string& base64, std::vector<uint8_t>& outRaw) {
    size_t neededLen = 0;
    // Probe call: dst=nullptr always reports MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL
    // and sets neededLen -- that "failure" is expected and intentionally ignored.
    mbedtls_base64_decode(nullptr, 0, &neededLen, (const unsigned char*)base64.data(), base64.size());
    if (neededLen == 0 || neededLen > 8192) return false;
    outRaw.resize(neededLen);
    size_t actualLen = 0;
    if (mbedtls_base64_decode(outRaw.data(), outRaw.size(), &actualLen,
                               (const unsigned char*)base64.data(), base64.size()) != 0) {
        return false;
    }
    outRaw.resize(actualLen);
    return true;
}
}  // namespace

CanvasClockface::CanvasClockface(Adafruit_GFX* display, const char* themeJson) : display_(display) {
    JsonDocument doc;
    if (deserializeJson(doc, themeJson)) {
        valid_ = false;
        return;
    }

    bgColor_ = doc["bgColor"] | 0;
    int delay = doc["delay"] | 300;
    if (delay < 20) delay = 20;  // guard against a busy-loop from a 0ms theme
    delayMs_ = (uint16_t)delay;

    for (JsonVariantConst item : doc["setup"].as<JsonArrayConst>()) {
        const char* type = item["type"] | "";
        SetupElement el;
        el.x = item["x"] | 0;
        el.y = item["y"] | 0;
        el.x1 = item["x1"] | 0;
        el.y1 = item["y1"] | 0;
        el.width = item["width"] | 0;
        el.height = item["height"] | 0;
        el.color = item["color"] | 0;
        el.fgColor = item["fgColor"] | 0xFFFF;
        el.bgColor = item["bgColor"] | 0;
        el.font = std::string(item["font"] | "");

        if (strcmp(type, "rect") == 0) {
            el.type = SetupElement::RECT;
        } else if (strcmp(type, "fillrect") == 0) {
            el.type = SetupElement::FILLRECT;
        } else if (strcmp(type, "line") == 0) {
            el.type = SetupElement::LINE;
        } else if (strcmp(type, "text") == 0) {
            el.type = SetupElement::TEXT;
            el.content = std::string(item["content"] | "");
        } else if (strcmp(type, "datetime") == 0) {
            el.type = SetupElement::DATETIME;
            el.content = std::string(item["content"] | "H:i");
        } else if (strcmp(type, "image") == 0) {
            el.type = SetupElement::IMAGE;
            el.content = std::string(item["image"] | "");
        } else {
            continue;  // unrecognized element type: skip, don't crash
        }
        setupElements_.push_back(el);
    }

    // sprites[i] is normally itself an array of {"image": "..."} frames (for
    // multi-frame animation, e.g. Nyan Cat's rainbow trail) but a custom
    // theme could provide a bare {"image": "..."} object for a single-frame
    // sprite -- handle both shapes.
    for (JsonVariantConst spriteEntry : doc["sprites"].as<JsonArrayConst>()) {
        std::vector<SpriteFrame> frames;
        if (spriteEntry.is<JsonArrayConst>()) {
            for (JsonVariantConst frameVar : spriteEntry.as<JsonArrayConst>()) {
                const char* img = frameVar["image"] | "";
                SpriteFrame frame;
                if (img[0] != '\0' && loadSpriteFrame(img, frame)) {
                    frames.push_back(frame);
                }
            }
        } else {
            const char* img = spriteEntry["image"] | "";
            SpriteFrame frame;
            if (img[0] != '\0' && loadSpriteFrame(img, frame)) {
                frames.push_back(frame);
            }
        }
        loadedSprites_.push_back(std::move(frames));
    }

    for (JsonVariantConst item : doc["loop"].as<JsonArrayConst>()) {
        const char* type = item["type"] | "";
        if (strcmp(type, "sprite") != 0) continue;
        LoopSprite ls;
        ls.spriteIndex = item["sprite"] | -1;
        ls.x = item["x"] | 0;
        ls.y = item["y"] | 0;
        loopSprites_.push_back(ls);
    }

    valid_ = true;
}

CanvasClockface::~CanvasClockface() {
    for (auto& frames : loadedSprites_) {
        for (auto& frame : frames) {
            if (frame.pixels != nullptr) heap_caps_free(frame.pixels);
        }
    }
}

bool CanvasClockface::loadSpriteFrame(const std::string& base64, SpriteFrame& outFrame) {
    std::vector<uint8_t> raw;
    if (!base64DecodeGuarded(base64, raw)) return false;

    if (g_png.openRAM(raw.data(), (int)raw.size(), pngDrawCallback) != PNG_SUCCESS) return false;
    int w = g_png.getWidth();
    int h = g_png.getHeight();
    if (w <= 0 || h <= 0 || w > 64 || h > 64) {
        g_png.close();
        return false;
    }

    uint16_t* pixels = (uint16_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (pixels == nullptr) {
        g_png.close();
        return false;
    }

    g_pngDrawDirect = false;
    g_pngDecodeDst = (uint8_t*)pixels;
    g_pngDecodeDstW = (uint16_t)w;
    g_pngDecodeDstH = (uint16_t)h;
    bool ok = (g_png.decode(nullptr, 0) == PNG_SUCCESS);
    g_png.close();
    if (!ok) {
        heap_caps_free(pixels);
        return false;
    }

    outFrame.pixels = pixels;
    outFrame.width = (uint16_t)w;
    outFrame.height = (uint16_t)h;
    return true;
}

void CanvasClockface::renderShape(const SetupElement& el) {
    switch (el.type) {
        case SetupElement::RECT:
            display_->drawRect(el.x, el.y, el.width, el.height, el.color);
            break;
        case SetupElement::FILLRECT:
            display_->fillRect(el.x, el.y, el.width, el.height, el.color);
            break;
        case SetupElement::LINE:
            display_->drawLine(el.x, el.y, el.x1, el.y1, el.color);
            break;
        default:
            break;
    }
}

void CanvasClockface::setFontByName(const std::string& name) {
    if (name == "picopixel") {
        display_->setFont(&Picopixel);
    } else if (name == "square") {
        display_->setFont(&atariFont);
    } else if (name == "big") {
        display_->setFont(&hour8pt7b);
    } else if (name == "medium") {
        display_->setFont(&minute7pt7b);
    } else {
        display_->setFont();
    }
}

void CanvasClockface::renderText(const std::string& text, const SetupElement& el) {
    setFontByName(el.font);
    int16_t x1, y1;
    uint16_t w, h;
    display_->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    display_->fillRect(el.x + x1, el.y + y1, w, h, el.bgColor);
    display_->setTextColor(el.fgColor);
    display_->setCursor(el.x, el.y);
    display_->print(text.c_str());
}

void CanvasClockface::renderImageElement(const SetupElement& el) {
    std::vector<uint8_t> raw;
    if (!base64DecodeGuarded(el.content, raw)) return;
    if (g_png.openRAM(raw.data(), (int)raw.size(), pngDrawCallback) != PNG_SUCCESS) return;
    g_pngDrawDirect = true;
    g_pngDrawTarget = display_;
    g_pngDrawX = el.x;
    g_pngDrawY = el.y;
    g_png.decode(nullptr, 0);
    g_png.close();
}

void CanvasClockface::renderDatetimeElements() {
    if (dateTime_ == nullptr) return;
    int hour = dateTime_->getHour();
    int minute = dateTime_->getMinute();
    int second = dateTime_->getSecond();
    int day = dateTime_->getDay();
    int month = dateTime_->getMonth();
    int year = dateTime_->getYear();
    for (const auto& el : setupElements_) {
        if (el.type != SetupElement::DATETIME) continue;
        renderText(formatEzTime(el.content, hour, minute, second, day, month, year), el);
    }
}

void CanvasClockface::setup(CWDateTime* dateTime) {
    dateTime_ = dateTime;
    display_->fillScreen(bgColor_);
    for (const auto& el : setupElements_) {
        switch (el.type) {
            case SetupElement::RECT:
            case SetupElement::FILLRECT:
            case SetupElement::LINE:
                renderShape(el);
                break;
            case SetupElement::TEXT:
                renderText(el.content, el);
                break;
            case SetupElement::IMAGE:
                renderImageElement(el);
                break;
            case SetupElement::DATETIME:
                break;  // drawn below, same code path as every later refresh
        }
    }
    renderDatetimeElements();
    lastLoopMs_ = millis();
    lastDateTimeMs_ = millis();
}

void CanvasClockface::update() {
    unsigned long now = millis();
    if (now - lastLoopMs_ >= delayMs_) {
        lastLoopMs_ = now;
        for (auto& ls : loopSprites_) {
            if (ls.spriteIndex < 0 || (size_t)ls.spriteIndex >= loadedSprites_.size()) continue;
            auto& frames = loadedSprites_[(size_t)ls.spriteIndex];
            if (frames.empty()) continue;
            const SpriteFrame& frame = frames[ls.currentFrame % frames.size()];
            if (frame.pixels != nullptr) {
                display_->drawRGBBitmap(ls.x, ls.y, frame.pixels, frame.width, frame.height);
            }
            if (frames.size() > 1) ls.currentFrame = (ls.currentFrame + 1) % frames.size();
        }
    }
    if (now - lastDateTimeMs_ >= 1000) {
        lastDateTimeMs_ = now;
        renderDatetimeElements();
    }
}
```

- [ ] **Step 3: Compile-check (no upload yet)**

Run:
```bash
cd firmware && arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc DeskMatrix
```
Expected: compiles clean (this file isn't wired into `ClockScreen.cpp` yet, so it compiles but isn't reachable/used — that's fine, confirms syntax/types/includes are all correct in isolation before Task 5 wires it up). If it fails on a missing symbol from `atariFont`/`hour8pt7b`/`minute7pt7b`/`Picopixel`, re-check Task 3's font vendoring step.

- [ ] **Step 4: Run the native suite to confirm no regressions**

Run: `bash tests/native/run_tests.sh`
Expected: all suites still pass (this task adds no native-testable code, only Arduino-only code).

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/screens/clockfaces/canvas/CanvasClockface.h firmware/DeskMatrix/CanvasClockface.cpp
git commit -m "add CanvasClockface: JSON-driven clockface interpreter"
```

---

### Task 5: Wire Canvas into `ClockScreen.cpp`

**Files:**
- Modify: `firmware/DeskMatrix/ClockScreen.cpp`

**Interfaces:**
- Consumes: `CanvasClockface` (Task 4), `AppConfig.canvasJson` (Task 2, via the existing global `AppConfig appConfig;` declared in `DeskMatrix.ino`), `kNyanCatJson`/`kStarWarsJson` (Task 3).
- Produces: `loadClockFace()` now accepts `"canvas"`, `"nyancat"`, `"starwars"` in addition to the existing `"mario"`/`"words"`/`"pacman"`. Signature (`void loadClockFace(const std::string& name)`, declared in `screens/ClockScreen.h`) is unchanged.

- [ ] **Step 1: Add the includes**

Edit `firmware/DeskMatrix/ClockScreen.cpp` — add after the existing clockface includes:

```cpp
#include "screens/clockfaces/canvas/CanvasClockface.h"
#include "screens/clockfaces/canvas/presets/nyancat.json.h"
#include "screens/clockfaces/canvas/presets/starwars.json.h"
#include "ConfigModel.h"
```

And add, alongside the existing `extern MatrixPanel_I2S_DMA* dma_display;`:

```cpp
// Declared in DeskMatrix.ino. loadClockFace("canvas") needs the user's
// pasted theme JSON, which lives here rather than being passed as a
// parameter -- mirrors how this file already reaches for dma_display.
extern AppConfig appConfig;
```

- [ ] **Step 2: Replace `loadClockFace()`'s body**

Edit `firmware/DeskMatrix/ClockScreen.cpp` — replace the existing `loadClockFace` function body:

```cpp
void loadClockFace(const std::string& name) {
  std::string resolved = name;
  if (resolved != "words" && resolved != "pacman" && resolved != "canvas" &&
      resolved != "nyancat" && resolved != "starwars") {
    resolved = "mario";
  }
  if (g_activeClockface != nullptr && resolved == g_activeName) return; // already active: no-op

  if (!g_dateTimeStarted) {
    g_dateTime.begin("", true);
    g_dateTimeStarted = true;
  }

  delete g_activeClockface; // IClockface has a virtual destructor -- see lib/cw-commons/IClockface.h
  g_activeClockface = nullptr;

  if (dma_display == nullptr) return; // called before initPanel(); caller error -- bail safely, retried on next drawClockFrame()

  // Clockfaces draw onto the persistent canvas, not dma_display directly --
  // see g_canvas's comment above.
  g_canvas.fillScreen(0);
  if (resolved == "words") {
    g_activeClockface = new WordsClockface(&g_canvas);
  } else if (resolved == "pacman") {
    g_activeClockface = new PacmanClockface(&g_canvas);
  } else if (resolved == "nyancat") {
    g_activeClockface = new CanvasClockface(&g_canvas, kNyanCatJson);
  } else if (resolved == "starwars") {
    g_activeClockface = new CanvasClockface(&g_canvas, kStarWarsJson);
  } else if (resolved == "canvas") {
    auto* canvasFace = new CanvasClockface(&g_canvas, appConfig.canvasJson.c_str());
    if (!canvasFace->isValid()) {
      // Malformed/empty pasted theme: fall back to Mario rather than a
      // blank/corrupt screen.
      delete canvasFace;
      g_activeClockface = new MarioClockface(&g_canvas);
      resolved = "mario";
    } else {
      g_activeClockface = canvasFace;
    }
  } else {
    g_activeClockface = new MarioClockface(&g_canvas);
  }
  g_activeClockface->setup(&g_dateTime);
  g_activeName = resolved;
}
```

- [ ] **Step 3: Compile-check**

Run:
```bash
cd firmware && arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc DeskMatrix
```
Expected: compiles clean.

- [ ] **Step 4: Run the native suite to confirm no regressions**

Run: `bash tests/native/run_tests.sh`
Expected: all suites pass.

- [ ] **Step 5: Commit**

```bash
git add firmware/DeskMatrix/ClockScreen.cpp
git commit -m "wire CanvasClockface into loadClockFace (canvas/nyancat/starwars)"
```

---

### Task 6: Config page UI

**Files:**
- Modify: `firmware/DeskMatrix/ConfigServer.cpp`

**Interfaces:**
- Consumes: `AppConfig.canvasJson` (Task 2), the existing `clockFace` dropdown and its save-on-change JS pattern.
- Produces: no new interfaces (UI-only change).

- [ ] **Step 1: Add the dropdown options and the custom-theme textarea section**

Edit `firmware/DeskMatrix/ConfigServer.cpp` — extend the existing clock face `<select>`:

```html
<select id="clockFace" style="width:100%;padding:.5em;margin-bottom:.4em;box-sizing:border-box">
<option value="mario">Mario</option>
<option value="words">Words</option>
<option value="pacman">Pacman</option>
<option value="nyancat">Nyan Cat</option>
<option value="starwars">Star Wars</option>
<option value="canvas">Canvas (custom)</option>
</select>
<div class="status" id="clockFaceStatus"></div>
<div id="canvasJsonSection" style="display:none;margin-top:.8em">
<label>Custom theme JSON (paste a theme from github.com/jnthas/clock-club)</label>
<textarea id="canvasJson" rows="8" style="width:100%;padding:.5em;box-sizing:border-box;font-family:monospace;font-size:.8em"></textarea>
<button type="button" id="canvasJsonSaveBtn" style="margin-top:.5em">Save custom theme</button>
<div class="status" id="canvasJsonStatus"></div>
</div>
```

(This replaces the existing three-option `<select id="clockFace">...</select>` block and the `<div class="status" id="clockFaceStatus"></div>` line right after it, adding the three new options and the new section in between.)

- [ ] **Step 2: Add the visibility-toggle helper and wire it into the existing load/save JS**

Edit `firmware/DeskMatrix/ConfigServer.cpp` — add before `getConfig().then(cfg => { ... });`:

```js
function updateCanvasSectionVisibility(clockFaceValue) {
  document.getElementById('canvasJsonSection').style.display = (clockFaceValue === 'canvas') ? 'block' : 'none';
}
```

In the existing `getConfig().then(cfg => { ... });` block, add two lines (after the existing `clockFace`/`timezone` assignments):

```js
  document.getElementById('canvasJson').value = cfg.canvasJson || '';
  updateCanvasSectionVisibility(cfg.clockFace || 'mario');
```

In the existing `clockFace` change handler, add the visibility call as its first line:

```js
document.getElementById('clockFace').addEventListener('change', async e => {
  updateCanvasSectionVisibility(e.target.value);
  const status = document.getElementById('clockFaceStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.clockFace = e.target.value;
    const res = await putConfig(cfg);
    status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
    status.className = res.ok ? 'status ok' : 'status err';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});
```

Add a new handler for the custom-theme Save button, after the `clockFace` change handler:

```js
document.getElementById('canvasJsonSaveBtn').addEventListener('click', async () => {
  const status = document.getElementById('canvasJsonStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.canvasJson = document.getElementById('canvasJson').value;
    const res = await putConfig(cfg);
    if (res.ok) {
      status.textContent = 'Saved.'; status.className = 'status ok';
    } else {
      const body = await res.json().catch(() => ({}));
      status.textContent = 'Failed: ' + (body.error || 'invalid JSON'); status.className = 'status err';
    }
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});
```

- [ ] **Step 3: Compile-check**

Run:
```bash
cd firmware && arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc DeskMatrix
```
Expected: compiles clean.

- [ ] **Step 4: Commit**

```bash
git add firmware/DeskMatrix/ConfigServer.cpp
git commit -m "add Canvas clockface options and custom-theme textarea to config page"
```

---

### Task 7: Hardware verification

**Files:** none (verification only)

**Interfaces:** none (final integration check)

- [ ] **Step 1: Run the full native test suite**

Run: `bash tests/native/run_tests.sh`
Expected: all 9 suites pass (the original 8 plus `test_ez_time_format`), no regressions.

- [ ] **Step 2: Compile and flash**

Run:
```bash
cd firmware && arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc DeskMatrix
ls /dev/cu.usbmodem*
arduino-cli upload -p /dev/cu.usbmodem<port> --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc DeskMatrix
```
Expected: compiles and uploads without error (check flash/RAM usage delta is small, consistent with Task 3's ~30-50KB flash estimate).

- [ ] **Step 3: Verify each bundled clockface on the config page**

Open the config page, select each of `Mario`, `Words`, `Pacman` (regression check — confirm no double-buffer flashing or lag reappeared), then `Nyan Cat` and `Star Wars`. For each:
- Confirm it renders without flashing/corruption (the persistent-canvas fix from the original Clock screen work still applies here unchanged).
- Confirm the clock digits show the correct current time.
- For Nyan Cat specifically: confirm the rainbow trail sprites animate (cycle frames) rather than sitting static — if they don't move, check `loadSpriteFrame()`'s frame-count logic and `update()`'s frame-advance line.
- For Star Wars specifically: confirm the clock digits render in the blocky "square" font, not the default font — if not, check `setFontByName()`'s `"square"` branch and that `fonts/atari.h` vendored correctly in Task 3.
- If any sprite's colors look channel-swapped (red/blue reversed): change `PNG_RGB565_BIG_ENDIAN` to `PNG_RGB565_LITTLE_ENDIAN` in `CanvasClockface.cpp`'s `pngDrawCallback`, recompile, reflash, and re-verify.

- [ ] **Step 4: Verify the custom-paste path**

Select `Canvas (custom)`, paste a real theme's JSON (e.g. fetch `https://raw.githubusercontent.com/jnthas/clock-club/main/shared/space-invaders.json` and paste its contents) into the textarea, click "Save custom theme". Confirm:
- The status shows "Saved." and the clock screen switches to the pasted theme, rendering correctly.
- Reloading the config page shows the pasted JSON still populates the textarea (round-tripped through `AppConfig.canvasJson`).

- [ ] **Step 5: Verify malformed-paste rejection**

With `Canvas (custom)` still selected, replace the textarea content with clearly invalid JSON (e.g. `{not json`) and click "Save custom theme". Confirm:
- The status shows a "Failed: ..." message (not a silent success).
- The clock screen keeps showing whatever theme was active before (the bad save is rejected server-side by `parseConfig`, so `AppConfig` — and therefore the active clockface — never actually changes).

- [ ] **Step 6: Regression check on unrelated features**

Confirm Spotify play/pause, DND/BRB tilt, sleep toggle, and the 5-minute GIF interlude still behave exactly as before (all `SCREENSAVER`-mode guards and the `loop()` structure are untouched by this feature).

- [ ] **Step 7: Final commit (if Step 3's endianness fix or any other fix was needed)**

```bash
git add -A
git commit -m "fix Canvas clockface issues found during hardware verification"
```

(Skip this step if no fixes were needed during verification.)

---

## Self-Review Notes

- **Spec coverage:** paste-in textarea (Task 6) ✓, full PNG support (Task 4) ✓, `CanvasClockface`/`IClockface` integration (Tasks 4-5) ✓, bundled Nyan Cat + Star Wars presets (Task 3, 5) ✓, ezTime datetime translation (Task 1) ✓, 64KB size cap + structural validation (Task 2) ✓, runtime resilience / fallback-to-Mario (Task 4, 5) ✓, native tests for pure-logic pieces (Tasks 1, 2) ✓, hardware verification (Task 7) ✓, budget estimate (Task 3's flash note, spec's RAM/PSRAM budget realized via `MALLOC_CAP_SPIRAM` in Task 4) ✓.
- **Refinement beyond the spec, discovered during research:** the spec described "sprite" elements generically; investigating clock-club's actual Nyan Cat theme showed `sprites[]` entries are arrays-of-frames (for animation), which Task 4 handles via per-frame decoding and cycling in `update()`. This wasn't a scope change — sprite animation was implicitly part of "loop array elements... redrawn every delay ms" in the approved spec — just an implementation detail that needed nailing down against real data.
- **Type consistency checked:** `CanvasClockface`'s constructor signature, `isValid()`, and field names are used identically in Task 4 (definition) and Task 5 (call site). `formatEzTime()`'s signature matches between Task 1's declaration, test, and Task 4's call site. `AppConfig.canvasJson` name matches across Tasks 2, 5, 6.
- **No placeholders:** every code block is complete, real code or an exact, deterministic command — no "TODO"/"add appropriate X" anywhere.
