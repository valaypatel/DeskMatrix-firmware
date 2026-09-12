# Home Screen v2: Dual Clocks, Day Display, and Icon Rendering — Design

Date: 2026-08-08
Builds on: `docs/superpowers/specs/2026-08-08-led-matrix-platform-design.md` (MVP platform, already implemented and running on device)

## Overview

Expands the Home dashboard's top 64×32 strip to show a local clock (with
day-of-week), a remote clock (London), and a weather display — all with
icons (national flags per clock, weather condition icon). This requires one
genuinely new capability: an **icon rendering pipeline**. Today,
`POST /api/assets` can store a bitmap, but nothing ever reads one back and
draws it — every widget so far is text-only. This spec adds that pipeline
alongside the layout and clock changes.

The bottom 64×32 stays reserved/empty — out of scope for this spec.

## Layout

```
┌─────────────────────┬─────────────────────┐
│ 🇮🇳 10:42 AM          │                     │
│    SAT               │        ☀            │
├───────────────────────        29°C          │
│ 🇬🇧 11:12 PM          │                     │
└─────────────────────┴─────────────────────┘
        (top 64×32 — the only zone this spec touches)
┌───────────────────────────────────────────┐
│                                             │
│           reserved / empty                 │
│                                             │
└─────────────────────────────────────────────┘
        (bottom 64×32 — untouched, out of scope)
```

- Left column (32×32): local clock on top (12h + AM/PM, flag, day-of-week
  underneath), remote clock on bottom (12h + AM/PM, flag)
- Right column (32×32): weather icon + temperature, large and centered

## Config schema additions

Additive only — nothing existing changes shape.

```json
{ "id": "w1", "type": "clock", "style": "digital_with_day", "color": "#FFDE59",
  "icon": "flag_in", "x": 0, "y": 0, "w": 32, "h": 16 },
{ "id": "w4", "type": "clock", "style": "digital", "color": "#FFDE59",
  "icon": "flag_gb", "location": "51.5074,-0.1278",
  "x": 0, "y": 16, "w": 32, "h": 16 },
{ "id": "w2", "type": "weather", "style": "icon_temp", "color": "#4C9AFF",
  "icon": "weather_auto", "x": 32, "y": 0, "w": 32, "h": 32, "dataSource": "ds_weather" }
```

- **`WidgetConfig.location`** (new, optional): a `"lat,lon"` string, same
  format already used by `dataSources`. Present → this clock is a "remote"
  clock (see mechanism below). Absent → existing local-time behavior,
  unchanged.
- **`style: "digital_with_day"`** (new clock style, alongside the existing
  `"digital"`): renders the day-of-week abbreviation under the time. Only
  the local clock uses this — the remote clock keeps the plain `"digital"`
  style, since the day is shown once, not per-clock (per design discussion:
  it's usually the same day for both, so one indicator is enough).
- **`icon`** (existing field, was parsed but never drawn): now consumed by
  the new icon rendering pipeline. `"flag_in"`, `"flag_gb"` for the two
  clocks; `"weather_auto"` for the weather widget signals "pick the icon
  from the current condition," not a fixed asset id (see mapping below).

## Remote clock timezone mechanism

A second, independent, low-frequency service (`RemoteClockService`, mirrors
`WeatherService`'s shape) queries Open-Meteo for the remote location's
coordinates with `timezone=auto`, reading only `utc_offset_seconds` from the
response (weather fields in that response are ignored). Polled every 6 hours,
not every 10 minutes like weather — timezone offsets only change at DST
transitions (twice a year), so frequent polling has no value.

The remote clock's draw function computes its own time independently of the
device's global local-time setting:
```
time_t utcNow = time(nullptr);          // always UTC, unaffected by configTime()'s offset
time_t remoteNow = utcNow + remoteOffsetSec;
struct tm remoteTm;
gmtime_r(&remoteNow, &remoteTm);        // breaks down the shifted value with NO further offset applied
```
This sidesteps the fact that only one timezone can be globally configured via
`configTime()` (already used for the local clock, driven by the weather
location's own `utc_offset_seconds` — unchanged from the MVP spec).

Rejected alternatives: hardcoding London's offset (silently wrong twice a
year at DST transitions); embedding a full IANA timezone database (far too
large for this chip for one remote clock — pure over-engineering).

## Icon rendering pipeline

**Storage** (unchanged from the existing asset API): each icon is one
canonical **64×64 RGB565 bitmap**, uploaded via the existing
`POST /api/assets?id=<icon_id>` to `/assets/<icon_id>.bin` — no API changes
needed, this already works, it's just never been read back until now.

**New: `drawIcon(display, iconId, x, y, w, h)`** — a small, generic,
reusable function:
1. Opens `/assets/<iconId>.bin` from LittleFS
2. For each of the `w × h` output pixels, nearest-neighbor-samples the
   corresponding pixel from the 64×64 source (`srcX = outX * 64 / w`,
   `srcY = outY * 64 / h`) and calls `display->drawPixel(x+outX, y+outY, ...)`
3. If the asset file doesn't exist (not yet uploaded), draws nothing and
   returns — a widget with a missing icon shows blank, not a crash

One function, used by every icon everywhere: flags at ~12×8 within their
clock's cell, the weather icon larger within its 32×32 cell. No per-size
asset duplication — matches the platform's original code/config boundary
(icons are data, drawn generically by code that doesn't know or care what
the picture is).

**Weather icon selection:** `WeatherService`'s Open-Meteo request adds
`is_day` to the `current` fields (needed to distinguish clear-day from
clear-night). A small lookup table maps WMO `weather_code` ranges (already
fetched) plus `is_day` to one of four icon ids: `weather_sunny`,
`weather_cloudy`, `weather_rainy`, `weather_clear_night`. The weather
widget's `drawIcon` call uses whichever id this mapping resolves to, not a
fixed config value — this is why `icon: "weather_auto"` in config is a
sentinel, not a literal asset id.

**Asset sourcing for this spec:** flags (India, UK) and the four weather
condition icons are sourced from a standard open flag/icon set, converted to
64×64 RGB565 with the same SVG→bitmap pipeline already used for the Jira and
fire icons earlier in this project, and previewed for approval before
upload — same workflow as before, no new tooling needed.

## Clock format change

Both clocks switch from the MVP's 24-hour `HH:MM` to 12-hour `H:MM AM/PM`
(shorter form `H:MMA`/`H:MMP` if width is tight once real font metrics are
measured on-device — confirm exact form during implementation, since this
is the kind of thing that's only truly verifiable by looking at the real
panel).

## Testing

- `drawIcon`'s nearest-neighbor scaling math (source/output pixel mapping)
  is pure logic — testable natively, independent of the actual LittleFS
  read or `display->drawPixel()` calls, by injecting a fake source buffer
  and a fake pixel-sink function
- `RemoteClockService`'s epoch+offset→`gmtime_r()` computation is pure
  logic — testable natively with fixed epoch/offset inputs and expected
  `tm` output, no network or hardware needed
- The weather_code/is_day → icon id mapping table is pure logic — testable
  natively as a lookup function
- Everything else (actual panel rendering, actual LittleFS reads, actual
  Open-Meteo calls) is on-device/compile-verified only, per the MVP
  platform's established testing approach
