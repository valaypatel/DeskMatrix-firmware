# Office Desk Display — Screensaver + Spotify — Design

Date: 2026-09-06
Hardware: Waveshare ESP32-S3-RGB-Matrix (ESP32-S3-N32R16, 32MB flash, 16MB OPI
PSRAM) + 64×64 HUB75 panel — same physical device as the home desk platform,
repurposed for the office.

## Overview

The home desk build (clock + weather dashboard, see
`2026-08-08-led-matrix-platform-design.md`) is being replaced for office use.
The office device has no small-widget dashboard: it shows one thing
full-screen at a time — a looping GIF screensaver by default, or your
Spotify album art whenever something is actively playing. `DND`/`BRB`
tilt modes are unchanged and still override both.

This spec removes the Home dashboard entirely (clock, weather, and their
supporting services) rather than leaving it dormant, since this device will
not run both use cases.

## Scope

**In scope for this phase:**
- GIF screensaver as the default resting state — one GIF active at a time,
  uploaded over HTTP, looped continuously
- Spotify "now playing" full-screen album art, shown automatically whenever
  your account is actively playing, reverting to the screensaver when
  playback stops
- Removal of the clock/weather Home dashboard, `WeatherService`,
  `RemoteClockService`, and the `home.widgets` / `dataSources` config schema
- `DND`/`BRB` tilt modes carried over unchanged

**Explicitly out of scope (future work):**
- "Event pages" (Jira, calendar, other notification-style full-screen
  takeovers) — will reuse the `INTERRUPT_TAKEOVER` scaffolding from the
  original design, designed separately when a concrete first event exists
- Track/artist text overlay on the Spotify screen — plain RGB565 text on a
  64×64 panel reads poorly at typical font sizes; worth trying later as a
  polish pass, not a requirement for this phase
- Multiple simultaneous screensaver GIFs / rotation — only one GIF is ever
  active
- Authentication on the HTTP API — unchanged from the original design,
  local network only

## Architecture

### Display modes (state machine)

```
WIFI_SETUP → SCREENSAVER ⇄ SPOTIFY_PLAYING
                  │              │
       IMU tilt   │              │   IMU tilt
       (left)     ▼              ▼   (right)
                DND / BRB
     (overrides SCREENSAVER or SPOTIFY_PLAYING;
      resumes whichever was active when tilt returns to center)
```

- `WIFI_SETUP` — captive portal, unchanged
- `SCREENSAVER` — default resting state (replaces `HOME`); loops the single
  currently-uploaded GIF indefinitely
- `SPOTIFY_PLAYING` — entered automatically the instant a poll reports
  `isPlaying: true`; shows full-screen album art. Exited automatically back
  to `SCREENSAVER` the instant a poll reports `isPlaying: false`, or after
  repeated poll failures.
- `DND` / `BRB` — unchanged from the original design: IMU-tilt triggered,
  override whatever mode was active, resume it on return to center

`SPOTIFY_PLAYING` is **level-triggered** by a continuously-polled external
condition, the same pattern as `DND`/`BRB` — not the one-shot
`INTERRUPT_TAKEOVER` FIFO queue from the original design. That queue concept
remains valid for future one-shot event pages but is unused here.

### Spotify integration

- **Libraries**: `SpotifyArduino` (witnessmenow) handles the OAuth
  refresh-token → access-token exchange and the "currently playing" polling
  call. `JPEGDEC` (bitbank2) decodes the returned album art JPEG directly
  onto the panel via a draw callback — no manual resize needed, since
  Spotify's API returns multiple art sizes and the smallest is already 64×64.
- **Credentials**: `clientId`, `clientSecret`, and `refreshToken` are stored
  in the device's config (pushed via `PUT /api/config`, persisted to
  LittleFS) — not hardcoded in firmware source. This matches how every other
  piece of device config already works and keeps secrets out of source
  control.
- **One-time OAuth bootstrap**: done once, outside the firmware entirely —
  open a Spotify authorize link in any browser (phone is fine), log in, and
  a small one-off local script exchanges the resulting code for a refresh
  token. That refresh token is then pushed to the device via the config API
  and never needs the browser step again unless access is revoked.
- **Polling**: every 5 seconds by default (config field, same pattern as the
  old `pollSec` for weather) while idle-checking; frequent enough for album
  art changes to feel responsive without hammering Spotify's API the way a
  1-second poll loop would.
- **Change detection**: only re-fetch and re-decode art when the track's
  album art URL actually changes, so pause/resume of the same song doesn't
  cause a visible flash or wasted decode.

### GIF screensaver

- **Library**: `AnimatedGIF` (bitbank2) — same family as `JPEGDEC`, a proven
  pairing for HUB75 panels.
- **Upload**: new endpoint `POST /api/screensaver` — raw GIF bytes as the
  request body, written to a fixed LittleFS path (e.g. `/screensaver.gif`),
  always overwriting whatever was there. No `id` parameter, since exactly
  one GIF is ever active — this is deliberately simpler than the multi-icon
  `POST /api/assets/:id` pattern.
- **Playback**: decoded frame-by-frame in a loop while in `SCREENSAVER`
  state. On interruption (Spotify starts playing, or tilt triggers
  `DND`/`BRB`) and later return to `SCREENSAVER`, playback simply restarts
  from the first frame — no need to persist exact frame position.
- **No GIF uploaded yet**: `SCREENSAVER` state shows a blank/black screen
  rather than erroring, consistent with the platform's "never brick the
  display" philosophy.

### Removed from the codebase

- `ClockWidget.h/.cpp`, `WeatherWidget.h/.cpp`, `WeatherService`,
  `RemoteClockService`, and the `ds_weather` data source concept
- The `home.widgets` / `dataSources` sections of the config schema and the
  small-widget `WidgetRegistry` dispatch model that supported them
- The generic `IconWidget` added during the home-desk build is superseded by
  the dedicated Spotify/screensaver draw paths and is removed rather than
  kept as unused code

## Data model (config schema)

```json
{
  "wifi": { "configured": true },
  "spotify": {
    "clientId": "...",
    "clientSecret": "...",
    "refreshToken": "...",
    "pollSec": 5
  },
  "dnd": { "art": "dnd_default" },
  "brb": { "art": "brb_default" }
}
```

`home`, `screens`, and `dataSources` are removed entirely — they described
concepts (widget dashboard, generic data sources) that no longer apply to
this device.

## HTTP API & persistence

- `GET /api/config` — read current config (now just `wifi`/`spotify`/`dnd`/`brb`)
- `PUT /api/config` — replace whole config; validate; persist to LittleFS;
  apply live. Malformed config rejected (400), last-valid config stays active.
- `POST /api/screensaver` — upload a new screensaver GIF (raw bytes),
  overwrites the previous one. Malformed/corrupt GIF rejected (400), previous
  screensaver (if any) stays active.
- `POST /api/ota` — unchanged, accepts a compiled firmware binary
- `POST /api/assets/:id` — retained for `dnd`/`brb` art bitmaps, which still
  use the existing static-icon pipeline

**Boot sequence** (unchanged apart from dropping the weather/clock init):
1. If no saved Wi-Fi credentials: open `DeskMatrix-Setup` captive portal
2. Connect using saved/entered credentials
3. Show IP address on the panel briefly
4. Load config from flash
5. Enter `ScreenStateMachine` at `SCREENSAVER`

## Error handling

- **Spotify API failure/timeout**: treated identically to "not playing" —
  falls back to `SCREENSAVER` rather than showing a stale or broken image
- **JPEG decode failure** (corrupt response, unexpected format): same
  fallback to `SCREENSAVER`, logged to Serial
- **No refresh token configured**: Spotify polling is simply skipped; device
  runs the screensaver indefinitely until Spotify config is pushed
- **Malformed config PUT**: rejected (400), last-valid config remains active
- **Malformed GIF upload**: rejected (400), previous screensaver (if any)
  remains active
- **IMU unavailable/init failure**: `DND`/`BRB` disabled for that boot, rest
  of the system unaffected (unchanged from original design)

## Testing approach

- Spotify polling and album art rendering verified against a real Spotify
  account/session on the physical panel (no practical way to mock Spotify's
  API meaningfully for this hardware-bound behavior)
- GIF upload and playback verified with a handful of real GIFs of varying
  frame count/size to confirm looping and memory behavior
- Config API changes (new `spotify` block, removal of `home`/`dataSources`)
  exercised via direct HTTP calls before any client UI exists, same as the
  original design
- Mode transitions (`SCREENSAVER` ⇄ `SPOTIFY_PLAYING`, tilt into/out of
  `DND`/`BRB` from both states) checked manually on the real device
