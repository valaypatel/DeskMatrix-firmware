# LED Matrix Display Platform — Design

Date: 2026-08-08
Hardware: Waveshare ESP32-S3-RGB-Matrix (ESP32-S3-N32R16, 32MB flash, 16MB OPI PSRAM) + 64×64 HUB75 panel

## Overview

The ESP32 hosts a small display platform: a "Home" dashboard of mini-widgets,
plus additional full-panel "screens" that can temporarily take over the
display when their data changes. The device is configured remotely (mobile/web
client, built later) by pushing JSON over HTTP — the firmware never needs to
be touched for everyday configuration changes.

This spec covers the on-device platform only. The mobile/web configuration
client is a separate, later project that will be built against the HTTP API
defined here.

## Scope

**In scope for this MVP:**
- Home dashboard showing two widgets: **Clock** and **Weather**
- Do Not Disturb (DND) and Be Right Back (BRB) modes, triggered by physically
  tilting the device (persistent orientation, read from the onboard IMU)
- Config, asset, and OTA HTTP API
- The interrupt/takeover state machine and FIFO queue, scaffolded and ready
  for future data sources — but unused in this MVP since Clock and Weather
  don't need to interrupt Home

**Explicitly out of scope (future work):**
- Jira, Calendar, and other data sources — added later as new widget/screen
  types once this foundation is proven; each is additive, not a redesign
- The mobile/web configuration app (a "web config studio") — a separate
  project built against the API defined here, once it's stable
- Tilt-left/right navigation between multiple full screens (MVP2)
- A real plugin interface/dynamic plugin install — revisit once 2-3 data
  sources exist and the real shared shape is known
- Authentication on the HTTP API — local network, single trusted client,
  acceptable gap for now

## Architecture

### Display modes (state machine)

```
WIFI_SETUP → HOME ⇄ INTERRUPT_TAKEOVER
                │
     IMU tilt   │   IMU tilt
     (left)     ▼   (right)
             DND / BRB
   (overrides whatever HOME/INTERRUPT_TAKEOVER
    state was active; returns to it when tilt
    goes back to center)
```

A single `ScreenStateMachine` owns what's currently on the panel and why.
Modes:
- `WIFI_SETUP` — captive portal active (already built and validated)
- `HOME` — the dashboard of widgets (default resting state)
- `INTERRUPT_TAKEOVER` — a full-panel screen shown because its data source
  flagged a change; drains a FIFO queue, one screen at a time, each for its
  configured `durationSec`, then returns to `HOME`
- `DND` / `BRB` — full-panel artistic screens, active for as long as the
  device is physically tilted left/right of center (IMU-read, debounced).
  Override any other mode; on return to center, resume whatever was active
  underneath.

### Widget/screen registry

Each widget or screen has a `type` (e.g. `"clock"`, `"weather"`) resolved
through a small compiled-in registry mapping type → draw function. This is
where the code/config line sits:

| Change | Needs code + OTA? |
|---|---|
| Position, size, color, text content, poll interval, credentials | No — pure config |
| Selecting between pre-built style variants (e.g. digital vs analog clock) | No, once the style exists — pure config |
| A style that doesn't exist yet (e.g. a new clock face) | Yes, one-time — after that it's config-selectable forever |
| A new icon/image | No — icons are pre-rasterized 64×64 bitmaps uploaded as assets, never firmware |
| A new widget/screen *type* (e.g. first-ever Jira support) | Yes — new type needs new code |

MVP builds one style per widget type (digital clock, one weather layout) to
keep scope tight; the `style` field is included in the schema now so adding a
second style later is additive, not a schema redesign.

### Data sources

Each data source (e.g. `ds_weather`) is defined once, independently of the
widgets/screens that use it, and referenced by id. This lets a Home
mini-widget and a full-screen takeover share one set of credentials/config
without duplicating it. For MVP, only `ds_weather` exists.

Weather provider: **Open-Meteo** (no API key required, free, sufficient for
a single-location poll every ~10 minutes) — recommended default, can be
swapped later without changing the schema shape.

### IMU polling

Read continuously (~100–200ms interval), thresholded/debounced so normal desk
bumps don't false-trigger DND/BRB. Orientation crossing the left/right
threshold sets `DND`/`BRB`; returning to center clears it.

## Data model (config schema)

```json
{
  "wifi": { "configured": true },
  "home": {
    "widgets": [
      { "id": "w1", "type": "clock", "style": "digital", "color": "#FFDE59",
        "x": 0, "y": 0, "w": 24, "h": 10 },
      { "id": "w2", "type": "weather", "style": "icon_temp", "icon": "weather_sunny",
        "color": "#4C9AFF", "x": 0, "y": 40, "w": 16, "h": 16, "dataSource": "ds_weather" }
    ]
  },
  "screens": [],
  "dataSources": {
    "ds_weather": { "type": "weather", "pollSec": 600, "location": "..." }
  },
  "dnd": { "art": "dnd_default" },
  "brb": { "art": "brb_default" }
}
```

`screens` is present and supported by the state machine but starts empty —
the first future data source (e.g. Jira) adds an entry here plus an
`interrupt: { enabled, durationSec }` block, without any other schema change.

## HTTP API & persistence

- `GET /api/config` — read current config
- `PUT /api/config` — replace whole config; validate; persist to flash
  (LittleFS); apply live. Malformed config is rejected (400) and the last
  valid config stays active — a bad push never bricks the display.
- `POST /api/assets?id=...` — upload a new icon/image bitmap (binary, 64×64,
  RGB565), stored in flash, referenced by id from widget/screen config. (The
  id is passed as a query parameter rather than a path segment because the
  ESP32 `WebServer` library used by the firmware has no path-parameter
  support.)
- `POST /api/ota` — accept a compiled firmware binary, flash, reboot

**Boot sequence** (Wi-Fi provisioning already built and validated):
1. If no saved Wi-Fi credentials: open `DeskMatrix-Setup` captive portal
2. Connect using saved/entered credentials
3. Show IP address on the panel for 10 seconds
4. Load config from flash
5. Enter `ScreenStateMachine` at `HOME`

## Error handling

- **Data source fetch failure** (Wi-Fi hiccup, API error): widget/screen
  keeps showing its last-known-good value; after N consecutive failures,
  shows a small "stale" indicator rather than going blank
- **Malformed config PUT**: rejected with 400; last-valid config remains
  active
- **IMU unavailable/init failure**: DND/BRB simply disabled for that boot;
  rest of the system unaffected

## Known MVP limitations

- **Timezone is a compile-time constant**: `TIMEZONE_OFFSET_SEC` in
  `firmware/DeskMatrix/config.h` is baked into the firmware image at build
  time; it is not part of the config schema above and cannot be changed via
  `PUT /api/config`. This means a timezone change currently requires a
  firmware rebuild and reflash, contradicting this spec's stated goal that
  "the firmware never needs to be touched for everyday configuration
  changes." Making timezone config-driven (e.g. an added `"timezone"` field
  applied via `configTime()` at runtime) is a follow-up, not yet implemented.

## Testing approach

- Each widget/screen draw function testable in isolation by feeding it fixed
  data (no live network dependency) and visually checking output on the real
  panel
- Config API exercised via curl/Postman before any client UI exists —
  proves the API is usable standalone, independent of any client
- IMU threshold/debounce tuned empirically on the real device (desk bumps vs.
  intentional tilt)
