# DeskMatrix Desktop Emulator — Design

## Goal

Let screen/clockface development happen without the physical ESP32-S3 board
connected. The real board stays in place and is only flashed (via the
existing Arduino CLI/IDE/OTA workflow) when a change is ready to deploy.
The emulator is a separate, parallel desktop build of the same source files,
not a replacement for the hardware build.

## Scope (v1)

In scope:

- Clock faces: Mario, Words, Pacman, Canvas (JSON custom theme)
- GIF screensaver (assets read from a local folder instead of LittleFS)
- DND / BRB static screens
- Spotify screen with hardcoded fake track/album-art data (layout only —
  no real network calls)

Out of scope for v1 (deferred):

- Real Spotify network calls from the desktop build
- Running the real `ConfigServer` web UI on desktop
- IMU tilt simulation beyond a keyboard hotkey
- OTA / config-persistence testing (that's what the real board is for)

## Architecture

Two build targets compile largely the same source files:

- **Hardware target** (unchanged): `firmware/DeskMatrix/DeskMatrix.ino`,
  built via Arduino CLI/IDE, flashed to the ESP32-S3 as today.
- **Native target** (new): `emulator/`, a CMake project that compiles the
  same screen and clockface `.cpp` files against an SDL2-backed display and
  a minimal Arduino-compatibility shim, producing a desktop executable.

This works because the clockfaces (`MarioClockface`, `WordsClockface`,
`PacmanClockface`, `CanvasClockface`) already draw through `Adafruit_GFX*`,
not the ESP32-specific panel class — they're already portable. Only the
outer screen functions and `DeskMatrix.ino` currently depend on
`MatrixPanel_I2S_DMA*` directly, and their actual usage of it is narrow:
standard `Adafruit_GFX` methods (`drawPixel`, `fillScreen`, `drawRGBBitmap`,
`setCursor`, `setTextColor`, `setTextSize`, `setTextWrap`, `print`,
`color565`) plus one HUB75-specific extension, `flipDMABuffer()`.

### Changes to shared firmware code

1. Retype these function signatures from `MatrixPanel_I2S_DMA*` to
   `Adafruit_GFX*` (no other changes to their bodies):
   - `screens/ClockScreen.h` — `drawClockFrame()`
   - `screens/ScreensaverScreen.h` — `drawScreensaverFrame()`,
     `drawDndGifFrame()`
   - `screens/SpotifyScreen.h` — `drawSpotifyScreen()`
   - `screens/DndScreen.h` — `drawDndScreen()`
   - `screens/BrbScreen.h` — `drawBrbScreen()`
2. Add a free function `void presentFrame(Adafruit_GFX* display);`
   (declared in a new small header, e.g. `PanelPresent.h`) and replace the
   ~8 call sites of `display->flipDMABuffer()` in `ClockScreen.cpp`,
   `ScreensaverScreen.cpp`, `SpotifyScreen.cpp`, and `DeskMatrix.ino` with
   `presentFrame(display)`.
   - Hardware implementation (`PanelPresent.cpp`, compiled only in the
     Arduino build): `static_cast<MatrixPanel_I2S_DMA*>(display)->flipDMABuffer();`
   - Native implementation (`emulator/NativePanel.cpp`): calls the
     `NativePanel`'s own present/render step (see below).
   `DeskMatrix.ino`'s `dma_display` variable stays `MatrixPanel_I2S_DMA*` —
   only calls made *through the shared screen functions'* `display`
   parameter go through `presentFrame()`.

No other changes to existing screen/clockface logic are needed.

## Native Build Components (`emulator/`)

**1. Arduino compatibility shim** (`emulator/arduino_shim/Arduino.h` and a
few small companion headers). Provides only what the shared `.cpp` files
actually need on desktop:
- `uint8_t`/`uint16_t`/etc. (from `<cstdint>`)
- `PROGMEM`, `pgm_read_byte`, `pgm_read_word`, `pgm_read_dword` — become
  plain dereferences (no PROGMEM/flash distinction on x86)
- `millis()`, `delay()`, `micros()` — via `<chrono>`/`<thread>`
- `Print` base class: `virtual size_t write(uint8_t)` plus `print()` /
  `println()` overloads implemented in terms of `write()`
- `Serial` — a stub object whose `print`/`println` write to stdout
- `F()` macro — identity (returns the same `const char*`)

This is enough for `Adafruit_GFX`, `GFXcanvas16`, `gfxfont.h`, the vendored
clockface fonts, and `AnimatedGIF` to compile unchanged — none of them touch
networking or ESP32-specific hardware APIs, only this narrow "Arduino-ness."

**2. `NativePanel`** (`emulator/NativePanel.h/.cpp`) — an `Adafruit_GFX`
subclass wrapping an SDL2 window/texture:
- Implements `drawPixel()`, the one abstract method `Adafruit_GFX`
  requires, by writing into an in-memory RGB565 buffer sized to the panel
  (`PANEL_RES_X` × `PANEL_RES_Y`).
- `present()` uploads that buffer to an SDL2 texture and renders it scaled
  up (e.g. 8×) so the 64×64 panel is visible on a normal monitor. This is
  what `presentFrame()`'s native implementation calls.

**3. Asset loading** — `ScreensaverScreen.cpp`'s GIF/theme loading currently
reads via LittleFS. A `NativeFS` shim (`emulator/NativeFS.h/.cpp`) exposes
the same handful of file-open/read calls the GIF-decoding and
theme-loading code uses, backed by plain file I/O against a local
`emulator/assets/` folder — no changes to the decoding logic itself. Copy a
few sample GIFs and a sample custom-theme JSON into `emulator/assets/` for
local testing.

**4. Fake data providers**:
- `emulator/fake_config.json` — same shape as the real device's config
  (clockface choice, brightness, etc.), loaded once at startup.
- A hardcoded fake Spotify track/album-art struct passed to
  `drawSpotifyScreen()`, replacing the real `SpotifyService` network call
  for this v1 scope.

**5. Entry point** (`emulator/main.cpp`) — initializes SDL2, loads
`fake_config.json`, and runs a loop calling the same
`drawClockFrame()` / `drawScreensaverFrame()` / `drawDndScreen()` /
`drawBrbScreen()` / `drawSpotifyScreen()` functions the real firmware
calls. Keyboard hotkeys drive scenario switching:
- `1`/`2`/`3`/`4` — switch clockface (Mario/Words/Pacman/Canvas)
- `d` — trigger DND screen
- `b` — trigger BRB screen
- `s` — force the screensaver interlude
- `Esc`/window close — quit

## Build Tooling & Developer Workflow

CMake + SDL2 (installed via Homebrew: `brew install sdl2`). One-time setup
plus a single build+run command:

```bash
cmake -B emulator/build -S emulator
cmake --build emulator/build
./emulator/build/deskmatrix_emulator
```

No hot-reloading in v1 — edit code, rebuild, rerun. This matches how the
real firmware is iterated on today (compile → flash → observe) and keeps
the emulator's own build simple.

## Testing / Verification

- Manual visual check: run the emulator, cycle through all four clockfaces,
  trigger DND/BRB, force the screensaver, and confirm each renders
  correctly in the SDL2 window.
- The existing native test suite (`tools/`, run via `pytest`, per
  `README.md`'s Development section) is unrelated to this emulator and
  keeps running as-is — this is a new, separate way to *see* rendering
  output, not a replacement for existing tests.
- No automated visual-diff testing in v1 (out of scope — could be added
  later by dumping the `NativePanel` buffer to a PNG per frame and
  comparing against golden images).

## Impact on the Hardware Build

None functionally. The `Adafruit_GFX*` retyping is a strict widening (every
call site already only used methods `Adafruit_GFX` provides, except the
`flipDMABuffer()` calls now routed through `presentFrame()`), so the
Arduino/ESP32 compile output is unchanged. `presentFrame()`'s hardware
implementation is a one-line forwarding call. The CI workflow
(`.github/workflows/build.yml`) is unaffected — `emulator/` is not part of
the Arduino sketch and won't be picked up by `arduino-cli compile`.
