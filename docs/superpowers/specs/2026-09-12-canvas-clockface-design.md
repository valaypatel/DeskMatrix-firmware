# Canvas Clockface Design

## Context

DeskMatrix's idle screen already offers three ported Clockwise clockfaces
(Mario, Words, Pacman — see `docs/superpowers/plans/` clock-screen work) via
`ClockScreen.cpp`'s `loadClockFace(name)` and a config-page dropdown. When
that feature was built, a 4th "paste raw JSON custom clockface" slot was
explicitly deferred.

Investigating Clockwise's `cw-cf-0x07` ("Canvas") clockface and its
companion `jnthas/clock-club` theme repository revealed this deferred slot
maps directly onto an existing, well-specified upstream feature: Canvas is
a clockface that interprets a JSON theme file at runtime instead of being
hardcoded in C++. `clock-club`'s `shared/` folder has 13 ready-made themes
(Nyan Cat, Star Wars, Space Invaders, Donkey Kong, Pac-Man, Goomba Move,
Christmas Snoopy, Snoopy 3, Retro Computer, Pepsi Final 2, Umbrella
Corporation, Eletrogate, Clock Club), 1.4KB–13.5KB each.

This spec covers building that Canvas engine, exposing it as a "paste your
own JSON" config-page slot, and bundling two crowd-pleasing themes (Nyan
Cat, Star Wars) as zero-setup presets alongside it.

## Canvas JSON schema (upstream, unchanged)

```js
{
    "name": "My Theme", "version": 1, "author": "@jnthas",
    "bgColor": 0,        // decimal RGB565, background fill
    "delay": 300,        // ms between loop redraws
    "setup": [ /* drawn once at load */ ],
    "sprites": [ /* base64 PNG images, referenced by index */ ],
    "loop": [ /* redrawn every `delay` ms */ ]
}
```

`setup`/`loop` element types: `text`, `datetime`, `rect`, `fillrect`,
`line`, `image` (base64 PNG, setup only), `sprite` (references `sprites[]`
by index, loop only, for simple per-tick repositioning/animation).

## Architecture

One new `CanvasClockface` class, implementing the existing `IClockface`
interface exactly like `MarioClockface`/`WordsClockface`/`PacmanClockface`.
It is constructed from a JSON string and has no opinion about where that
string came from — the same class serves three config options:

- **`canvas`**: the user's pasted JSON, stored in `AppConfig.canvasJson`
  (persisted via the existing `SettingsStore`/LittleFS-backed config file —
  no size pressure there; the config partition is 24MB and today's whole
  config file is a few KB).
- **`nyancat`** / **`starwars`**: JSON vendored into firmware as
  `PROGMEM` string constants under
  `screens/clockfaces/canvas/presets/{nyancat,starwars}.json.h`, copied
  once from `clock-club` at build time (not fetched over network — works
  offline, immune to upstream repo changes).

`ClockScreen.cpp`'s `loadClockFace(name)` gains branches for all three,
constructing `CanvasClockface` with the appropriate JSON source. All three
draw onto the same persistent `GFXcanvas16` canvas already used by the
existing clockfaces — the double-buffer-safe full-redraw pattern requires
no changes.

**Parse once, draw many.** At `loadClockFace()` time (not per-frame), the
JSON is parsed into a small internal vector of prepared draw commands:
text/datetime/shape descriptors, plus sprite pixel buffers **decoded once
up front** (not re-decoded every frame). Every `update()` call just replays
that command list. This mirrors the fix for the earlier Mario jump-lag bug
(never redo expensive work inside the render hot path).

## Parsing & rendering pipeline

1. **JSON parse**: `ArduinoJson` `JsonDocument` (already a project
   dependency), one-shot. Largest known theme (~13.5KB) is trivial against
   217KB free heap.
2. **PNG decode** (for `image`/`sprites[]` entries): mbedtls's base64
   decoder (already linked in for HTTPS/TLS) decodes the string to raw PNG
   bytes; `PNGdec` (bitbank2 — same author/API shape as the project's
   existing `AnimatedGIF` dependency) decodes to RGB565 scanlines via
   callback. Decoded pixel buffers are allocated from PSRAM
   (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`) to avoid pressuring internal
   RAM used by WiFi/TLS/JSON-parse scratch space.
3. **Shape/text elements** (`rect`, `fillrect`, `line`, `text`) map directly
   to existing `Adafruit_GFX` primitives on `GFXcanvas16`.
4. **`datetime` formatting**: Canvas themes use ezTime format tokens (e.g.
   `H:i`), but `CWDateTime` already replaced ezTime with `strftime`-based
   formatting (see its port history). A small translator function walks the
   format string and maps the common ezTime tokens — `H`/`G`/`h`/`g` (hour
   variants), `i` (minute), `s` (second), `d`/`j` (day), `m`/`n` (month),
   `Y`/`y` (year) — to `CWDateTime`'s existing getters. Only tokens that
   actually appear in real clock-club themes need support, not the full
   ezTime spec.

## Config UI

- Clock face dropdown gains `Nyan Cat`, `Star Wars`, and `Canvas (custom)`.
- Selecting `Canvas (custom)` reveals a textarea (hidden otherwise),
  pre-filled with the currently saved `canvasJson`, with its own **explicit
  Save button** — unlike other fields' save-on-change, a multi-KB paste
  should not fire a PUT per keystroke.
- **Validation on save** (`parseConfig`): reject `canvasJson` over 64KB
  (comfortably above the largest known real theme), and reject anything
  that isn't valid JSON with a `setup` or `loop` array — same
  `{"error": "..."}` response shape the config API already uses.
- **Runtime resilience**: an individual malformed sprite (bad base64,
  undecodable PNG) or unrecognized element `type` is skipped with a log
  line, not a crash — the rest of the theme still renders. If the whole
  JSON fails to parse at load time, `loadClockFace()` falls back to Mario
  (today's default) rather than showing a blank/corrupt screen.

## Testing

- **Native tests** (`tests/native/`, no hardware needed): `AppConfig`
  parse/serialize round-trip for `canvasJson` including the size-cap
  rejection; a standalone test for the ezTime-token translator (pure
  string/logic, same style as `test_orientation_debouncer.cpp`).
- **Hardware verification**: flash and confirm Mario/Words/Pacman (regression
  check) plus Nyan Cat, Star Wars, and one hand-pasted shapes-only custom
  theme (e.g. Retro Computer, no images — exercises the non-PNG path) all
  render correctly; confirm an intentionally-broken paste is rejected with
  a clear error rather than breaking the clock screen.

## Budget

PNGdec + two bundled theme JSONs adds an estimated 30-50KB flash (current
firmware: 28% of 4.7MB app partition used, comfortable headroom) and a few
KB of transient parse RAM plus PSRAM-backed sprite buffers (current: 33% of
327KB internal RAM used, plus 16MB unused PSRAM). No meaningful resource
risk on this board.

## Out of scope

- Fetching theme JSON from a URL (clock-club's `raw.githubusercontent.com`
  pattern) — paste-only for this pass, per explicit decision.
- Any clock-club theme beyond Nyan Cat and Star Wars as bundled presets —
  everything else is available via paste.
- IMU-gesture triggers, the JIRA takeover screen, and any other previously
  deferred feature — unrelated to this spec.
