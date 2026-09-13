# DeskMatrix Desktop Emulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a desktop SDL2 emulator that compiles and runs DeskMatrix's actual screen/clockface `.cpp` files unchanged, so their visual behavior can be developed and checked without the physical ESP32-S3 board connected.

**Architecture:** Retype the narrow set of shared screen functions from the ESP32-specific `MatrixPanel_I2S_DMA*` to the already-portable `Adafruit_GFX*` (the base class clockfaces already use), add a tiny cross-platform `presentFrame()` shim for the one non-portable call (`flipDMABuffer()`), and add a new `emulator/` CMake project providing an SDL2-backed `Adafruit_GFX` subclass plus a minimal Arduino-compatibility shim so the same `.cpp` files compile for desktop.

**Tech Stack:** C++17, CMake, SDL2 (via Homebrew), Arduino/ESP32 core (hardware side, unchanged), real `Adafruit_GFX`/`AnimatedGIF` libraries reused unmodified on both targets.

## Global Constraints

- Per the approved spec (`docs/superpowers/specs/2026-09-13-desktop-emulator-design.md`), v1 scope is: clock faces (Mario/Words/Pacman/Canvas), GIF screensaver, DND/BRB screens, and Spotify screen with hardcoded fake data (no real network calls). Real Spotify networking, the real `ConfigServer` web UI, and IMU tilt simulation are explicitly out of scope for this plan.
- The hardware build must keep compiling and behaving identically after Task 1 — verify with a real `arduino-cli compile` after every change to shared firmware files.
- Panel size is fixed at 64×64 (`PANEL_RES_X`/`PANEL_RES_Y` in `firmware/DeskMatrix/config.h`) — hardcode `64` in emulator code rather than re-deriving it, matching how the existing screen `.cpp` files already do (e.g. `constexpr int kSize = 64;` in `ScreensaverScreen.cpp`).
- No hot-reloading, no automated visual-diff testing in v1 — manual visual verification via the running emulator window is the acceptance test for each task that touches rendering.
- `emulator/` is a new top-level directory, entirely separate from `firmware/DeskMatrix/`'s Arduino sketch — it must never be picked up by `arduino-cli compile` (it isn't, since arduino-cli only looks inside the sketch folder passed to it).

---

### Task 1: Make shared screen functions portable (hardware-side change)

**Files:**
- Modify: `firmware/DeskMatrix/screens/ClockScreen.h`
- Modify: `firmware/DeskMatrix/ClockScreen.cpp`
- Modify: `firmware/DeskMatrix/screens/ScreensaverScreen.h`
- Modify: `firmware/DeskMatrix/ScreensaverScreen.cpp`
- Modify: `firmware/DeskMatrix/screens/SpotifyScreen.h`
- Modify: `firmware/DeskMatrix/SpotifyScreen.cpp`
- Modify: `firmware/DeskMatrix/screens/DndScreen.h`
- Modify: `firmware/DeskMatrix/DndScreen.cpp`
- Modify: `firmware/DeskMatrix/screens/BrbScreen.h`
- Modify: `firmware/DeskMatrix/BrbScreen.cpp`
- Create: `firmware/DeskMatrix/PanelPresent.h`
- Create: `firmware/DeskMatrix/PanelPresent.cpp`
- Modify: `firmware/DeskMatrix/DeskMatrix.ino`

**Interfaces:**
- Produces: `void presentFrame(Adafruit_GFX* display);` (declared in `PanelPresent.h`) — hardware implementation casts to `MatrixPanel_I2S_DMA*` and calls `flipDMABuffer()`.
- Produces: `extern Adafruit_GFX* g_activeDisplay;` (declared in `PanelPresent.h`, defined in `DeskMatrix.ino`) — the currently-active display, set once the panel is initialized. `ClockScreen.cpp`'s `loadClockFace()` checks this instead of `dma_display` directly, so it has no ESP32-specific type dependency. `emulator/main.cpp` (Task 6) will set this too.
- Consumes (Task 6 depends on this): the five `draw*` functions below now take `Adafruit_GFX*` instead of `MatrixPanel_I2S_DMA*`.

This task only changes signatures and the one `dma_display`-dependent guard — no rendering logic changes. Verify at the end with a real hardware compile.

- [ ] **Step 1: Create `PanelPresent.h`**

```cpp
// firmware/DeskMatrix/PanelPresent.h
#pragma once
#include <Adafruit_GFX.h>

// The currently-active display. Set once by DeskMatrix.ino's initPanel()
// (hardware) or emulator/main.cpp (native), after the concrete display
// object is constructed. ClockScreen.cpp's loadClockFace() checks this
// instead of depending on the ESP32-specific MatrixPanel_I2S_DMA type
// directly, so it has no hardware-specific dependency beyond Adafruit_GFX.
extern Adafruit_GFX* g_activeDisplay;

// Presents whatever has been drawn into `display` to the physical/virtual
// screen. On hardware this is MatrixPanel_I2S_DMA::flipDMABuffer(); on the
// native/SDL2 target (see emulator/NativePanel.cpp) it uploads the pixel
// buffer to the window and renders it. Every shared screen .cpp file calls
// this instead of `display->flipDMABuffer()` directly, since
// flipDMABuffer() isn't part of Adafruit_GFX's portable interface.
void presentFrame(Adafruit_GFX* display);
```

- [ ] **Step 2: Create `PanelPresent.cpp` (hardware implementation)**

```cpp
// firmware/DeskMatrix/PanelPresent.cpp
#include "PanelPresent.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

Adafruit_GFX* g_activeDisplay = nullptr;

void presentFrame(Adafruit_GFX* display) {
    static_cast<MatrixPanel_I2S_DMA*>(display)->flipDMABuffer();
}
```

- [ ] **Step 3: Retype `ClockScreen.h` and fix `ClockScreen.cpp`'s display guard**

In `firmware/DeskMatrix/screens/ClockScreen.h`, replace:

```cpp
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
```

with:

```cpp
#include <Adafruit_GFX.h>
```

and replace:

```cpp
void drawClockFrame(MatrixPanel_I2S_DMA* display);
```

with:

```cpp
void drawClockFrame(Adafruit_GFX* display);
```

In `firmware/DeskMatrix/ClockScreen.cpp`, replace:

```cpp
extern MatrixPanel_I2S_DMA* dma_display;
```

with:

```cpp
#include "PanelPresent.h"
```

(remove the old `extern MatrixPanel_I2S_DMA* dma_display;` line and its preceding comment block entirely — `PanelPresent.h` already declares `g_activeDisplay` with an equivalent comment).

Then in `loadClockFace()`, replace:

```cpp
  if (dma_display == nullptr) return; // called before initPanel(); caller error -- bail safely, retried on next drawClockFrame()
```

with:

```cpp
  if (g_activeDisplay == nullptr) return; // called before the display is initialized; caller error -- bail safely, retried on next drawClockFrame()
```

Finally, change the function signature:

```cpp
void drawClockFrame(MatrixPanel_I2S_DMA* display) {
```

to:

```cpp
void drawClockFrame(Adafruit_GFX* display) {
```

and the `display->flipDMABuffer();` call inside it to `presentFrame(display);`.

- [ ] **Step 4: Retype `ScreensaverScreen.h`/`.cpp`**

In `firmware/DeskMatrix/screens/ScreensaverScreen.h`, change the `#include` from `<ESP32-HUB75-MatrixPanel-I2S-DMA.h>` to `<Adafruit_GFX.h>`, and change both:

```cpp
void drawScreensaverFrame(MatrixPanel_I2S_DMA* display);
void drawDndGifFrame(MatrixPanel_I2S_DMA* display);
```

to:

```cpp
void drawScreensaverFrame(Adafruit_GFX* display);
void drawDndGifFrame(Adafruit_GFX* display);
```

In `firmware/DeskMatrix/ScreensaverScreen.cpp`, add `#include "PanelPresent.h"` near the top, then change all three `MatrixPanel_I2S_DMA*` parameter types (`playCurrentFrame`, `drawScreensaverFrame`, `drawDndGifFrame`) to `Adafruit_GFX*`, and replace the `display->flipDMABuffer();` call inside `playCurrentFrame()` with `presentFrame(display);`.

- [ ] **Step 5: Retype `SpotifyScreen.h`/`.cpp`**

In `firmware/DeskMatrix/screens/SpotifyScreen.h`, change the `#include` to `<Adafruit_GFX.h>` and:

```cpp
void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl, bool isPlaying);
```

to:

```cpp
void drawSpotifyScreen(Adafruit_GFX* display, const std::string& albumArtUrl, bool isPlaying);
```

In `firmware/DeskMatrix/SpotifyScreen.cpp`, add `#include "PanelPresent.h"`, retype all three `MatrixPanel_I2S_DMA*` parameters (`renderRecordFrame`, `renderIdleFrame`, `drawSpotifyScreen`) to `Adafruit_GFX*`, and replace both `display->flipDMABuffer();` calls with `presentFrame(display);`.

- [ ] **Step 6: Retype `DndScreen.h`/`.cpp` and `BrbScreen.h`/`.cpp`**

In `firmware/DeskMatrix/screens/DndScreen.h` and `firmware/DeskMatrix/screens/BrbScreen.h`: there's currently no `#include` for the display type in these headers (they rely on it being included first via `DeskMatrix.ino`'s include order) — add `#include <Adafruit_GFX.h>` at the top of each, then change:

```cpp
void drawDndScreen(MatrixPanel_I2S_DMA* display);
```

and

```cpp
void drawBrbScreen(MatrixPanel_I2S_DMA* display);
```

to use `Adafruit_GFX*`. In `firmware/DeskMatrix/DndScreen.cpp` and `firmware/DeskMatrix/BrbScreen.cpp`, retype the function signature the same way (no `flipDMABuffer()` calls in these two files, so no `presentFrame()` change needed here).

- [ ] **Step 7: Wire up `g_activeDisplay` in `DeskMatrix.ino`**

Add `#include "PanelPresent.h"` near the top of `firmware/DeskMatrix/DeskMatrix.ino` (alongside the other `#include`s). In `initPanel()`, after the existing:

```cpp
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
```

add:

```cpp
  g_activeDisplay = dma_display;
```

- [ ] **Step 8: Verify the hardware build still compiles cleanly**

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc \
  --build-property "build.partitions=large_littlefs_32MB" \
  --build-property "upload.maximum_size=4718592" \
  --output-dir /tmp/emulator-task1-verify \
  firmware/DeskMatrix/DeskMatrix.ino
```

Expected: compiles successfully, reporting a firmware size close to the pre-change baseline (~1,465,993 bytes program storage, ~42,720 bytes RAM — a few dozen bytes of difference from the new `presentFrame()`/`g_activeDisplay` symbols is fine, a large jump is not).

- [ ] **Step 9: Commit**

```bash
git add firmware/DeskMatrix/PanelPresent.h firmware/DeskMatrix/PanelPresent.cpp \
  firmware/DeskMatrix/screens/ClockScreen.h firmware/DeskMatrix/ClockScreen.cpp \
  firmware/DeskMatrix/screens/ScreensaverScreen.h firmware/DeskMatrix/ScreensaverScreen.cpp \
  firmware/DeskMatrix/screens/SpotifyScreen.h firmware/DeskMatrix/SpotifyScreen.cpp \
  firmware/DeskMatrix/screens/DndScreen.h firmware/DeskMatrix/DndScreen.cpp \
  firmware/DeskMatrix/screens/BrbScreen.h firmware/DeskMatrix/BrbScreen.cpp \
  firmware/DeskMatrix/DeskMatrix.ino
git commit -m "refactor: retype shared screen functions to Adafruit_GFX* for emulator portability"
```

---

### Task 2: Arduino compatibility shim

**Files:**
- Create: `emulator/arduino_shim/Arduino.h`
- Create: `emulator/arduino_shim/WString.h`
- Create: `emulator/arduino_shim/Print.h`
- Create: `emulator/arduino_shim/Print.cpp`
- Create: `emulator/CMakeLists.txt`
- Create: `emulator/smoke_test.cpp`

**Interfaces:**
- Produces: `PROGMEM` macro, `millis()`, `random()`/`randomSeed()`, `Serial` global object, `String` class, `Print` base class — everything `Adafruit_GFX`, the clockfaces, and `CWDateTime` need from "Arduino-ness" that isn't networking or hardware I/O.
- Consumes: nothing from earlier tasks.

This is the first piece of `emulator/`. It's verified standalone (compiling a trivial program against real `Adafruit_GFX` + this shim) before any DeskMatrix screen code is involved, so failures are easy to localize.

- [ ] **Step 1: Create `emulator/arduino_shim/Print.h`**

```cpp
// emulator/arduino_shim/Print.h
#pragma once
#include <cstddef>
#include <cstdint>

// Minimal stand-in for Arduino's Print base class. Adafruit_GFX inherits
// from this and implements write(uint8_t) itself (routing characters
// through drawChar()) -- this shim only needs to provide the pure virtual
// write() plus the print()/println() convenience overloads that Adafruit_GFX
// and the clockfaces actually call.
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size);

    size_t print(const char* s);
    size_t print(int v);
    size_t println(const char* s);
    size_t println();
    size_t printf(const char* format, ...);
};
```

- [ ] **Step 2: Create `emulator/arduino_shim/Print.cpp`**

```cpp
// emulator/arduino_shim/Print.cpp
#include "Print.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

size_t Print::write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size; i++) n += write(buffer[i]);
    return n;
}

size_t Print::print(const char* s) {
    return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
}

size_t Print::print(int v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", v);
    return print(buf);
}

size_t Print::println(const char* s) {
    size_t n = print(s);
    n += write('\n');
    return n;
}

size_t Print::println() {
    return write('\n');
}

size_t Print::printf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return print(buf);
}
```

- [ ] **Step 3: Create `emulator/arduino_shim/WString.h` (the `String` class)**

```cpp
// emulator/arduino_shim/WString.h
#pragma once
#include <string>
#include <cstdio>

// Minimal stand-in for Arduino's String class -- just enough for
// MarioBlock.cpp (`String(_dateTime->getHour())`, `.length()`) and
// CWDateTime.cpp (`String(buffer)`, returned by value).
class String {
public:
    String() = default;
    String(const char* s) : s_(s ? s : "") {}
    String(int v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v);
        s_ = buf;
    }
    size_t length() const { return s_.length(); }
    const char* c_str() const { return s_.c_str(); }

private:
    std::string s_;
};
```

- [ ] **Step 4: Create `emulator/arduino_shim/Arduino.h`**

```cpp
// emulator/arduino_shim/Arduino.h
//
// Minimal Arduino-compatibility shim for the native/desktop emulator
// build. Provides only what Adafruit_GFX, AnimatedGIF, CWDateTime, and the
// DeskMatrix clockfaces actually need -- see
// docs/superpowers/specs/2026-09-13-desktop-emulator-design.md for the
// full survey this was derived from. Adafruit_GFX.cpp itself already
// falls back to portable pgm_read_byte/word/dword macros when none are
// defined (see its own #ifndef guards), so this shim deliberately leaves
// those undefined rather than duplicating that fallback.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <chrono>

#define PROGMEM

inline unsigned long millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<unsigned long>(
        duration_cast<milliseconds>(steady_clock::now() - start).count());
}

inline long random(long max) { return max > 0 ? std::rand() % max : 0; }
inline void randomSeed(unsigned long seed) { std::srand(static_cast<unsigned>(seed)); }

#include "Print.h"
#include "WString.h"

class HardwareSerialStub : public Print {
public:
    size_t write(uint8_t c) override {
        putchar(c);
        return 1;
    }
};
inline HardwareSerialStub Serial;
```

- [ ] **Step 5: Create `emulator/CMakeLists.txt`**

```cmake
# emulator/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(deskmatrix_emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

set(ARDUINO_SHIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/arduino_shim)
set(GFX_LIB_DIR "$ENV{HOME}/Documents/Arduino/libraries/Adafruit_GFX_Library")
set(ANIMATEDGIF_LIB_DIR "$ENV{HOME}/Documents/Arduino/libraries/AnimatedGIF")

add_executable(smoke_test
    smoke_test.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
)
target_include_directories(smoke_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
)
```

- [ ] **Step 6: Create `emulator/smoke_test.cpp`**

```cpp
// emulator/smoke_test.cpp
// Standalone compile-only smoke test for Task 2: confirms the Arduino
// compatibility shim is sufficient for the real Adafruit_GFX library to
// compile and run a trivial drawing operation. No SDL2 window yet -- that
// is Task 3.
#include <Adafruit_GFX.h>
#include <cstdio>

class TestCanvas : public Adafruit_GFX {
public:
    TestCanvas() : Adafruit_GFX(8, 8) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        lastX = x; lastY = y; lastColor = color;
    }
    int16_t lastX = -1, lastY = -1;
    uint16_t lastColor = 0;
};

int main() {
    TestCanvas canvas;
    canvas.fillScreen(0);
    canvas.drawPixel(3, 4, 0xFFFF);
    if (canvas.lastX == 3 && canvas.lastY == 4 && canvas.lastColor == 0xFFFF) {
        printf("smoke test OK\n");
        return 0;
    }
    printf("smoke test FAILED\n");
    return 1;
}
```

- [ ] **Step 7: Build and run the smoke test**

```bash
brew install sdl2   # if not already installed
cmake -B emulator/build -S emulator
cmake --build emulator/build --target smoke_test
./emulator/build/smoke_test
```

Expected: prints `smoke test OK` and exits with code 0. If it fails to compile, the error will point at exactly which Arduino symbol Adafruit_GFX needs that the shim doesn't yet provide.

- [ ] **Step 8: Commit**

```bash
git add emulator/
git commit -m "feat(emulator): add Arduino compatibility shim and smoke test"
```

---

### Task 3: NativePanel (SDL2-backed display)

**Files:**
- Create: `emulator/NativePanel.h`
- Create: `emulator/NativePanel.cpp`
- Modify: `emulator/CMakeLists.txt`
- Create: `emulator/panel_test.cpp`

**Interfaces:**
- Consumes: `Print`/`Arduino.h` shim from Task 2.
- Produces: `class NativePanel : public Adafruit_GFX` with `void present();` — the native counterpart to `presentFrame()`'s hardware branch (Task 1). `emulator/main.cpp` (Task 6) constructs one `NativePanel` and passes it as the `Adafruit_GFX*` to every `draw*` function.

- [ ] **Step 1: Create `emulator/NativePanel.h`**

```cpp
// emulator/NativePanel.h
#pragma once
#include <Adafruit_GFX.h>
#include <SDL2/SDL.h>
#include <cstdint>

// SDL2-backed Adafruit_GFX implementation: the native/desktop counterpart
// to the real MatrixPanel_I2S_DMA. Owns an in-memory RGB565 buffer sized to
// the panel and a scaled-up SDL2 window/texture for display. `present()` is
// what PanelPresent.cpp's native build calls in place of flipDMABuffer().
class NativePanel : public Adafruit_GFX {
public:
    NativePanel(int width, int height, int scale);
    ~NativePanel() override;

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void present();

    // Returns false once the user closes the window (checked in main.cpp's
    // loop to know when to exit).
    bool pollEvents();

private:
    int width_;
    int height_;
    int scale_;
    uint16_t* buffer_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    bool running_ = true;
};
```

- [ ] **Step 2: Create `emulator/NativePanel.cpp`**

```cpp
// emulator/NativePanel.cpp
#include "NativePanel.h"
#include <cstdlib>
#include <cstring>

NativePanel::NativePanel(int width, int height, int scale)
    : Adafruit_GFX(width, height), width_(width), height_(height), scale_(scale) {
    buffer_ = static_cast<uint16_t*>(std::calloc(width_ * height_, sizeof(uint16_t)));

    SDL_Init(SDL_INIT_VIDEO);
    window_ = SDL_CreateWindow("DeskMatrix Emulator", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width_ * scale_, height_ * scale_,
                                SDL_WINDOW_SHOWN);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING, width_, height_);
}

NativePanel::~NativePanel() {
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
    std::free(buffer_);
}

void NativePanel::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    buffer_[y * width_ + x] = color;
}

void NativePanel::present() {
    SDL_UpdateTexture(texture_, nullptr, buffer_, width_ * sizeof(uint16_t));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

bool NativePanel::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) running_ = false;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running_ = false;
    }
    return running_;
}
```

- [ ] **Step 3: Add `emulator/panel_test.cpp`**

```cpp
// emulator/panel_test.cpp
// Manual visual check for Task 3: draws a red/green/blue striped pattern
// and holds the window open until closed. No DeskMatrix screen code
// involved yet -- purely verifies NativePanel itself.
#include "NativePanel.h"

int main() {
    NativePanel panel(64, 64, 8);
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            uint16_t color = 0;
            if (x < 21) color = 0xF800;       // red
            else if (x < 42) color = 0x07E0;  // green
            else color = 0x001F;              // blue
            panel.drawPixel(x, y, color);
        }
    }
    panel.present();
    while (panel.pollEvents()) {
        SDL_Delay(16);
    }
    return 0;
}
```

- [ ] **Step 4: Update `emulator/CMakeLists.txt`**

Add after the existing `smoke_test` target:

```cmake
add_executable(panel_test
    panel_test.cpp
    NativePanel.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
)
target_include_directories(panel_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
)
target_link_libraries(panel_test PRIVATE SDL2::SDL2)
```

Also add `target_link_libraries(smoke_test PRIVATE)` is not needed (no SDL2 dependency there) — leave `smoke_test`'s target as-is.

- [ ] **Step 5: Build and manually verify**

```bash
cmake -B emulator/build -S emulator
cmake --build emulator/build --target panel_test
./emulator/build/panel_test
```

Expected: a window opens showing three vertical stripes — red, green, blue, left to right — at 8x scale (512×512 pixels). Closing the window (or pressing Escape) exits cleanly.

- [ ] **Step 6: Commit**

```bash
git add emulator/
git commit -m "feat(emulator): add SDL2-backed NativePanel display"
```

---

### Task 4: NativeFS shim + GIF screensaver

**Files:**
- Create: `emulator/NativeFS.h`
- Create: `emulator/NativeFS.cpp`
- Modify: `emulator/CMakeLists.txt`
- Create: `emulator/assets/screensaver.gif` (a small sample GIF, any short looping animation ≤64×64)
- Create: `emulator/screensaver_test.cpp`

**Interfaces:**
- Consumes: `NativePanel` (Task 3), `presentFrame()`/`g_activeDisplay` (Task 1) — this task provides the *native* implementation of `presentFrame()`.
- Produces: a `LittleFS`-like global object and `File`-like class matching exactly what `ScreensaverScreen.cpp` calls (`LittleFS.open(path, "r")`, `LittleFS.exists(path)`, and on the returned `File`: `.size()`, `.read(buf, len)`, `.position()`, `.seek(pos)`, `.close()`, boolean conversion).

- [ ] **Step 1: Create `emulator/NativeFS.h`**

```cpp
// emulator/NativeFS.h
//
// Backs ScreensaverScreen.cpp's LittleFS.open()/.exists() calls with plain
// file I/O against a local directory (emulator/assets/) instead of the
// device's flash filesystem. Method names/signatures match exactly what
// ScreensaverScreen.cpp calls -- see gifOpen()/gifRead()/gifSeek() and
// loadPath() in that file.
#pragma once
#include <cstdio>
#include <cstdint>
#include <string>

class File {
public:
    File() = default;
    explicit File(FILE* f) : f_(f) {}

    operator bool() const { return f_ != nullptr; }

    size_t size() const {
        if (!f_) return 0;
        long pos = ftell(f_);
        fseek(f_, 0, SEEK_END);
        long end = ftell(f_);
        fseek(f_, pos, SEEK_SET);
        return static_cast<size_t>(end);
    }

    int read(uint8_t* buf, size_t len) {
        if (!f_) return 0;
        return static_cast<int>(fread(buf, 1, len, f_));
    }

    void seek(int32_t pos) {
        if (f_) fseek(f_, pos, SEEK_SET);
    }

    int32_t position() const {
        return f_ ? static_cast<int32_t>(ftell(f_)) : 0;
    }

    void close() {
        if (f_) { fclose(f_); f_ = nullptr; }
    }

private:
    FILE* f_ = nullptr;
};

class NativeFS {
public:
    // Matches LittleFS's real signature closely enough for this file's
    // call sites (mode is always "r" here).
    File open(const char* path, const char* mode) {
        std::string full = assetsDir_ + path;
        FILE* f = fopen(full.c_str(), mode);
        return File(f);
    }

    bool exists(const char* path) {
        std::string full = assetsDir_ + path;
        FILE* f = fopen(full.c_str(), "rb");
        if (f) { fclose(f); return true; }
        return false;
    }

    // Set once at emulator startup (main.cpp) to the emulator/assets/
    // directory, resolved relative to the running executable so it works
    // regardless of the current working directory.
    void setAssetsDir(const std::string& dir) { assetsDir_ = dir; }

private:
    std::string assetsDir_ = "./assets";
};

extern NativeFS LittleFS;
```

- [ ] **Step 2: Create `emulator/NativeFS.cpp`**

```cpp
// emulator/NativeFS.cpp
#include "NativeFS.h"

NativeFS LittleFS;
```

- [ ] **Step 3: Add the native implementation of `presentFrame()`**

Create `emulator/PanelPresentNative.cpp`:

```cpp
// emulator/PanelPresentNative.cpp
//
// Native counterpart to firmware/DeskMatrix/PanelPresent.cpp. Not compiled
// into the Arduino sketch (arduino-cli only looks inside
// firmware/DeskMatrix/) -- this is the emulator's own implementation of the
// same presentFrame()/g_activeDisplay contract declared in
// firmware/DeskMatrix/PanelPresent.h.
#include "../firmware/DeskMatrix/PanelPresent.h"
#include "NativePanel.h"

Adafruit_GFX* g_activeDisplay = nullptr;

void presentFrame(Adafruit_GFX* display) {
    static_cast<NativePanel*>(display)->present();
}
```

- [ ] **Step 4: Add a sample GIF asset**

```bash
mkdir -p emulator/assets
```

Place any small (≤64×64, ≤200KB) looping GIF at `emulator/assets/screensaver.gif`. If none is available locally, generate a minimal placeholder for testing purposes:

```bash
python3 -c "
from PIL import Image
frames = []
for i in range(8):
    img = Image.new('RGB', (64, 64), (0, 0, 0))
    for x in range(8):
        color = (255, 0, 0) if (x + i) % 2 == 0 else (0, 0, 255)
        for y in range(64):
            img.putpixel((x * 8 + (i % 8), y), color)
    frames.append(img)
frames[0].save('emulator/assets/screensaver.gif', save_all=True, append_images=frames[1:], duration=100, loop=0)
"
```

(Requires `pip3 install Pillow` if not already available. If Pillow isn't available and can't be installed, any existing small GIF file copied to `emulator/assets/screensaver.gif` works equally well for this verification step — the content doesn't matter, only that it decodes.)

- [ ] **Step 5: Create `emulator/screensaver_test.cpp`**

```cpp
// emulator/screensaver_test.cpp
// Manual visual check for Task 4: plays emulator/assets/screensaver.gif in
// a NativePanel window using the real ScreensaverScreen.cpp code, unchanged.
#include "NativePanel.h"
#include "../firmware/DeskMatrix/screens/ScreensaverScreen.h"

int main() {
    NativePanel panel(64, 64, 8);
    LittleFS.setAssetsDir("./assets");
    loadScreensaverGif();
    while (panel.pollEvents()) {
        drawScreensaverFrame(&panel);
        SDL_Delay(16);
    }
    return 0;
}
```

- [ ] **Step 6: Update `emulator/CMakeLists.txt`**

Add near the top (after `find_package(SDL2 REQUIRED)`):

```cmake
find_path(ANIMATEDGIF_SRC_DIR AnimatedGIF.h PATHS ${ANIMATEDGIF_LIB_DIR}/src)
```

Add a new target:

```cmake
add_executable(screensaver_test
    screensaver_test.cpp
    NativePanel.cpp
    NativeFS.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    ${ANIMATEDGIF_LIB_DIR}/src/AnimatedGIF.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix/ScreensaverScreen.cpp
)
target_include_directories(screensaver_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    ${ANIMATEDGIF_LIB_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix
)
target_link_libraries(screensaver_test PRIVATE SDL2::SDL2)
```

- [ ] **Step 7: Build and manually verify**

```bash
cmake -B emulator/build -S emulator
cmake --build emulator/build --target screensaver_test
cd emulator/build && ./screensaver_test
```

Expected: a window opens and plays the sample GIF on loop. Closing the window exits cleanly. If it fails to compile with missing symbols from `esp_heap_caps.h` (used by `ScreensaverScreen.cpp` for `heap_caps_malloc`/`MALLOC_CAP_SPIRAM`), add a stub header `emulator/arduino_shim/esp_heap_caps.h`:

```cpp
// emulator/arduino_shim/esp_heap_caps.h
#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 0
inline void* heap_caps_malloc(size_t size, int) { return std::malloc(size); }
```

and re-run the build.

- [ ] **Step 8: Commit**

```bash
git add emulator/
git commit -m "feat(emulator): add NativeFS shim, native presentFrame, and working GIF screensaver"
```

---

### Task 5: Spotify screen with fake data

**Files:**
- Create: `emulator/arduino_shim/WiFiClientSecure.h`
- Create: `emulator/arduino_shim/HTTPClient.h`
- Create: `emulator/arduino_shim/WiFiClient.h`
- Modify: `emulator/CMakeLists.txt`
- Create: `emulator/spotify_test.cpp`

**Interfaces:**
- Consumes: `NativePanel` (Task 3), `presentFrame()` (Task 4's native implementation).
- Produces: nothing new consumed by later tasks beyond confirming `SpotifyScreen.cpp` compiles and runs natively.

`SpotifyScreen.cpp` unconditionally includes `<WiFiClientSecure.h>`, `<HTTPClient.h>`, and `<JPEGDEC.h>` for its (real-network) album-art fetch path. Per the spec's v1 scope, this path is never meant to succeed on desktop — but the file still needs to *compile*, and `albumArtChanged()` returns `true` the first time a non-empty fake URL is passed in, so the network branch does get entered once per run. These stubs make that branch compile and fail gracefully (falling through to the idle-ring render), without needing a real network stack.

- [ ] **Step 1: Create `emulator/arduino_shim/WiFiClient.h`**

```cpp
// emulator/arduino_shim/WiFiClient.h
#pragma once
#include <cstdint>
#include <cstddef>

// Stub -- only exists so HTTPClient.h's getStreamPtr() return type and
// SpotifyScreen.cpp's stream->readBytes() call compile. Never actually
// used: HTTPClient::GET() below always returns a failure status, so
// SpotifyScreen.cpp's code never reaches the point of calling readBytes().
class WiFiClient {
public:
    int readBytes(uint8_t*, size_t) { return 0; }
};
```

- [ ] **Step 2: Create `emulator/arduino_shim/WiFiClientSecure.h`**

```cpp
// emulator/arduino_shim/WiFiClientSecure.h
#pragma once

// Stub -- satisfies SpotifyScreen.cpp's `WiFiClientSecure client;
// client.setInsecure();` call. No real TLS/networking on the native
// target for v1 (see docs/superpowers/specs/2026-09-13-desktop-emulator-design.md).
class WiFiClientSecure {
public:
    void setInsecure() {}
};
```

- [ ] **Step 3: Create `emulator/arduino_shim/HTTPClient.h`**

```cpp
// emulator/arduino_shim/HTTPClient.h
#pragma once
#include "WiFiClient.h"
#include "WiFiClientSecure.h"

// Stub -- GET() always reports failure (-1), so SpotifyScreen.cpp's
// drawSpotifyScreen() always takes its "fetch failed" branch and falls
// through to renderIdleFrame(). This is the intended v1 behavior: no real
// Spotify network calls from the desktop build.
class HTTPClient {
public:
    void begin(WiFiClientSecure&, const char*) {}
    int GET() { return -1; }
    WiFiClient* getStreamPtr() { return &stream_; }
    void end() {}

private:
    WiFiClient stream_;
};
```

- [ ] **Step 4: Create `emulator/spotify_test.cpp`**

```cpp
// emulator/spotify_test.cpp
// Manual visual check for Task 5: renders the Spotify screen with a fake
// (never-fetchable) album art URL using the real SpotifyScreen.cpp code,
// unchanged. Since the stub HTTPClient always fails, this always shows the
// idle ring (fillScreen ring pattern) -- the intended v1 behavior.
#include "NativePanel.h"
#include "../firmware/DeskMatrix/screens/SpotifyScreen.h"

int main() {
    NativePanel panel(64, 64, 8);
    while (panel.pollEvents()) {
        drawSpotifyScreen(&panel, "https://example.com/fake-album-art.jpg", true);
        SDL_Delay(16);
    }
    return 0;
}
```

- [ ] **Step 5: Update `emulator/CMakeLists.txt`**

Add a new target:

```cmake
find_path(JPEGDEC_LIB_DIR JPEGDEC.h PATHS "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src")

add_executable(spotify_test
    spotify_test.cpp
    NativePanel.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src/JPEGDEC.cpp"
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix/SpotifyScreen.cpp
)
target_include_directories(spotify_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix
)
target_link_libraries(spotify_test PRIVATE SDL2::SDL2)
```

- [ ] **Step 6: Build and manually verify**

```bash
cmake -B emulator/build -S emulator
cmake --build emulator/build --target spotify_test
./emulator/build/spotify_test
```

Expected: a window opens showing a faint idle ring outline (the "not playing / no album art" state). No crash, no network activity. Closing the window exits cleanly.

If the build fails because `services/SpotifyService.h` (included transitively by `SpotifyScreen.cpp`) pulls in additional Arduino/ESP32-only symbols beyond what's stubbed so far, check the specific missing symbol and add the smallest possible stub for it in `emulator/arduino_shim/` following the same pattern as Steps 1-3 — do not modify `SpotifyScreen.cpp` or `SpotifyService.h` themselves.

- [ ] **Step 7: Commit**

```bash
git add emulator/
git commit -m "feat(emulator): add network stubs and working Spotify screen (fake data)"
```

---

### Task 6: Clockfaces, DND/BRB, and the full emulator entry point

**Files:**
- Create: `emulator/fake_config.json`
- Create: `emulator/main.cpp`
- Modify: `emulator/CMakeLists.txt`

**Interfaces:**
- Consumes: everything from Tasks 1-5 — `NativePanel`, `NativeFS`/`LittleFS`, `presentFrame()`/`g_activeDisplay`, the stub network headers, plus (new in this task) the clockface `.cpp` files and `ClockScreen.cpp`/`DndScreen.cpp`/`BrbScreen.cpp`.
- Produces: the final `deskmatrix_emulator` executable — the deliverable this whole plan builds toward.

This is the integration task: build the real `deskmatrix_emulator` target compiling `ClockScreen.cpp` (and all four clockfaces, plus the `lib/cw-gfx-engine`/`lib/cw-commons` engine files), `ScreensaverScreen.cpp`, `DndScreen.cpp`, `BrbScreen.cpp`, and `SpotifyScreen.cpp` together, driven by keyboard input.

- [ ] **Step 1: Create `emulator/fake_config.json`**

```json
{
  "clockFace": "mario",
  "canvasJson": ""
}
```

- [ ] **Step 2: Create `emulator/main.cpp`**

```cpp
// emulator/main.cpp
//
// Desktop emulator entry point. Loads fake_config.json, then runs the same
// draw*() functions the real firmware's DeskMatrix.ino loop() calls,
// switched by keyboard hotkeys instead of ScreenStateMachine/IMU/Spotify
// polling. See docs/superpowers/specs/2026-09-13-desktop-emulator-design.md
// for the full design.
#include "NativePanel.h"
#include "NativeFS.h"
#include "../firmware/DeskMatrix/PanelPresent.h"
#include "../firmware/DeskMatrix/screens/ClockScreen.h"
#include "../firmware/DeskMatrix/screens/ScreensaverScreen.h"
#include "../firmware/DeskMatrix/screens/DndScreen.h"
#include "../firmware/DeskMatrix/screens/BrbScreen.h"
#include "../firmware/DeskMatrix/screens/SpotifyScreen.h"
#include "../firmware/DeskMatrix/ConfigModel.h"

#include <fstream>
#include <sstream>
#include <iostream>

// AppConfig is declared in ConfigModel.h; ClockScreen.cpp reaches for it via
// `extern AppConfig appConfig;` (see ClockScreen.cpp's loadClockFace()).
AppConfig appConfig;

enum class Scene { Clock, Screensaver, Dnd, Brb, Spotify };

int main() {
    LittleFS.setAssetsDir("./assets");

    // Minimal JSON load: fake_config.json only ever has the two fields
    // below for v1, so a full ArduinoJson dependency isn't needed here --
    // ConfigModel.cpp's real parseConfig() is not reused because it also
    // pulls in ArduinoJson, which isn't part of this plan's native shim
    // scope.
    {
        std::ifstream f("./fake_config.json");
        std::stringstream buffer;
        buffer << f.rdbuf();
        std::string contents = buffer.str();
        size_t pos = contents.find("\"clockFace\"");
        if (pos != std::string::npos) {
            size_t start = contents.find('"', contents.find(':', pos)) + 1;
            size_t end = contents.find('"', start);
            appConfig.clockFace = contents.substr(start, end - start);
        }
    }

    NativePanel panel(64, 64, 8);
    g_activeDisplay = &panel;

    loadClockFace(appConfig.clockFace);
    loadScreensaverGif();

    Scene scene = Scene::Clock;
    int screensaverLoops = 0;

    std::cout << "Keys: 1=Mario 2=Words 3=Pacman 4=NyanCat(preset) 5=Canvas(custom JSON)  d=DND b=BRB s=Screensaver c=Clock p=Spotify  Escape closes\n";

    while (panel.pollEvents()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type != SDL_KEYDOWN) continue;
            switch (event.key.keysym.sym) {
                case SDLK_1: appConfig.clockFace = "mario"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_2: appConfig.clockFace = "words"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_3: appConfig.clockFace = "pacman"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_4: appConfig.clockFace = "nyancat"; loadClockFace(appConfig.clockFace); scene = Scene::Clock; break;
                case SDLK_5:
                    // Exercises the actual "Canvas (custom)" paste-JSON path
                    // (appConfig.canvasJson), not just the built-in nyancat/
                    // starwars presets -- see loadClockFace()'s "canvas"
                    // branch in ClockScreen.cpp.
                    appConfig.canvasJson =
                        "{\"name\":\"Test\",\"bgColor\":0,\"delay\":50,"
                        "\"setup\":[{\"type\":\"text\",\"content\":\"HI\",\"x\":10,\"y\":10,\"fgColor\":65535}],"
                        "\"sprites\":[],\"loop\":[]}";
                    appConfig.clockFace = "canvas";
                    loadClockFace(appConfig.clockFace);
                    scene = Scene::Clock;
                    break;
                case SDLK_d: scene = Scene::Dnd; break;
                case SDLK_b: scene = Scene::Brb; break;
                case SDLK_s: scene = Scene::Screensaver; screensaverLoops = 0; break;
                case SDLK_c: scene = Scene::Clock; break;
                case SDLK_p: scene = Scene::Spotify; break;
                default: break;
            }
        }

        switch (scene) {
            case Scene::Clock:
                drawClockFrame(&panel);
                break;
            case Scene::Screensaver:
                drawScreensaverFrame(&panel);
                if (screensaverGifLoopCompleted()) {
                    screensaverLoops++;
                    if (screensaverLoops >= 10) scene = Scene::Clock;
                }
                break;
            case Scene::Dnd:
                drawDndScreen(&panel);
                break;
            case Scene::Brb:
                drawBrbScreen(&panel);
                break;
            case Scene::Spotify:
                drawSpotifyScreen(&panel, "https://example.com/fake-album-art.jpg", true);
                break;
        }

        SDL_Delay(16);
    }

    return 0;
}
```

- [ ] **Step 3: Update `emulator/CMakeLists.txt` with the full `deskmatrix_emulator` target**

Add at the end of the file:

```cmake
set(DESKMATRIX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix)

add_executable(deskmatrix_emulator
    main.cpp
    NativePanel.cpp
    NativeFS.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    ${ANIMATEDGIF_LIB_DIR}/src/AnimatedGIF.cpp
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src/JPEGDEC.cpp"
    ${DESKMATRIX_DIR}/ConfigModel.cpp
    ${DESKMATRIX_DIR}/ClockScreen.cpp
    ${DESKMATRIX_DIR}/ScreensaverScreen.cpp
    ${DESKMATRIX_DIR}/DndScreen.cpp
    ${DESKMATRIX_DIR}/BrbScreen.cpp
    ${DESKMATRIX_DIR}/SpotifyScreen.cpp
    ${DESKMATRIX_DIR}/CWDateTime.cpp
    ${DESKMATRIX_DIR}/EventBus.cpp
    ${DESKMATRIX_DIR}/Locator.cpp
    ${DESKMATRIX_DIR}/Sprite.cpp
    ${DESKMATRIX_DIR}/MarioClockface.cpp
    ${DESKMATRIX_DIR}/MarioBlock.cpp
    ${DESKMATRIX_DIR}/MarioSprite.cpp
    ${DESKMATRIX_DIR}/WordsClockface.cpp
    ${DESKMATRIX_DIR}/PacmanClockface.cpp
    ${DESKMATRIX_DIR}/PacmanClockfaceAi.cpp
    ${DESKMATRIX_DIR}/PacmanClockfaceMap.cpp
    ${DESKMATRIX_DIR}/PacmanClockfacePathfinding.cpp
    ${DESKMATRIX_DIR}/PacmanEntity.cpp
    ${DESKMATRIX_DIR}/PacmanGhost.cpp
    ${DESKMATRIX_DIR}/PacmanSprite.cpp
    ${DESKMATRIX_DIR}/CanvasClockface.cpp
)
target_include_directories(deskmatrix_emulator PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    ${ANIMATEDGIF_LIB_DIR}/src
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DESKMATRIX_DIR}
)
target_link_libraries(deskmatrix_emulator PRIVATE SDL2::SDL2)
```

Note: `ClockScreen.cpp` includes `"ConfigModel.h"` and uses `extern AppConfig appConfig;` — `ConfigModel.cpp` is included in the build above to provide the `AppConfig` struct's definition (from `ConfigModel.h`, header-only for the struct itself) and any helper functions `ClockScreen.cpp` might pull in transitively. If `ConfigModel.cpp` fails to compile natively due to an ArduinoJson dependency it has beyond what's already shimmed, remove it from this target's sources — `ClockScreen.cpp` only needs the `AppConfig` *type* (from the header) and the `appConfig`/`canvasJson` *values* (supplied by `main.cpp`'s own global `AppConfig appConfig;`), not `ConfigModel.cpp`'s `parseConfig()`/`serializeConfig()` functions.

- [ ] **Step 4: Build**

```bash
cmake -B emulator/build -S emulator
cmake --build emulator/build --target deskmatrix_emulator
cd emulator/build && ./deskmatrix_emulator
```

- [ ] **Step 5: Manual verification walkthrough**

With the emulator running:
1. Confirm it boots showing the Mario clockface (default).
2. Press `1`, `2`, `3`, `4`, `5` in turn — confirm the display switches to Mario, Words, Pacman, the Nyan Cat canvas preset, and finally the custom-JSON canvas path (showing "HI" text), each rendering correctly.
3. Press `d` — confirm the DND screen (dark red background, "DND" text) appears.
4. Press `b` — confirm the BRB screen (dark blue background, "BRB" text) appears.
5. Press `s` — confirm the screensaver GIF plays; after 10 loops, confirm it automatically returns to the clock (press `c` first if you don't want to wait, to confirm the manual return works, then press `s` again and wait through the full 10 loops at least once).
6. Press `p` — confirm the Spotify screen shows the idle ring (no crash, no network activity).
7. Close the window (or press Escape) — confirm the process exits cleanly with no crash or hang.

- [ ] **Step 6: Commit**

```bash
git add emulator/
git commit -m "feat(emulator): wire up full emulator entry point with clockfaces, DND/BRB, and hotkeys"
```

---

## Final Verification

- [ ] Re-run the hardware compile one more time to confirm nothing in `emulator/` accidentally affected it:
  ```bash
  arduino-cli compile \
    --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc \
    --build-property "build.partitions=large_littlefs_32MB" \
    --build-property "upload.maximum_size=4718592" \
    --output-dir /tmp/emulator-final-verify \
    firmware/DeskMatrix/DeskMatrix.ino
  ```
- [ ] Confirm `emulator/` is not picked up by the GitHub Actions workflow (`.github/workflows/build.yml`'s `paths:` filter only triggers on `firmware/**` and the workflow file itself — `emulator/` changes alone won't trigger a firmware build, which is correct).
- [ ] Run the full manual walkthrough from Task 6, Step 5 one final time against the final committed state.
