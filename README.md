# DeskMatrix Firmware

An ESP32-S3 LED matrix display platform for your desk — animated clocks, Spotify now-playing, and customizable screensavers.

## Features

- **Animated Clock Faces** — Mario, Words, and Pacman full-screen animated presets
- **Canvas (Custom JSON Themes)** — Paste a JSON theme from the [clock-club](https://github.com/jnthas/clock-club) gallery, or use the built-in Nyan Cat / Star Wars presets
- **GIF Screensaver** — Plays GIFs from flash storage; interrupts the idle clock every 5 minutes for 10 loops, then returns to the clock (configurable idle mode)
- **Spotify Integration** — Displays current track, artist, and album art as a spinning vinyl record
- **Web Configuration** — WiFi setup, live settings, DND/BRB/screensaver GIF uploads, and theme editing via HTTP
- **Do Not Disturb (DND) / BRB** — Physical tilt sensor triggers DND/BRB status screens with custom GIF art
- **OTA Firmware Updates** — Upload new firmware over WiFi from the web UI, no USB cable required
- **Automated Builds** — Every push to `main` is compiled and published as a GitHub Release

## Hardware

### Specifications

- **Microcontroller** — ESP32-S3-N32R16 (32MB Flash, 16MB OPI PSRAM)
- **Display** — 64×64 HUB75 RGB LED Matrix (2.5mm pitch)
- **Sensor** — 6-axis IMU (optional, for tilt-based gestures)
- **Programming** — USB-C via CDC

### Bill of Materials

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32-S3-N32R16 Dev Board | 1 | Microcontroller with PSRAM |
| 64×64 HUB75 RGB LED Matrix | 1 | 2.5mm pitch, ~1000-5000 nits |
| 5V Power Supply | 1 | **20A+ recommended** for full brightness (see Power Supply Notes below) |
| HUB75 Cable (or DIY ribbon) | 1 | 16-pin connection to matrix |
| Mounting Bracket/Enclosure | 1 | Optional — 3D printed or custom |
| 6-Axis IMU (QMI8658 or similar) | 1 | Optional — for tilt gesture detection |

### Power Supply Notes

The HUB75 matrix's power draw varies by content:
- **Idle/Clock**: 2-5A at 5V
- **Animated GIF**: 5-15A at 5V
- **Full White Brightness**: 15-20A at 5V

**Recommended:** 5V 20A power supply with quality wiring and a capacitor across the supply leads to smooth inrush current.

Common sources: Amazon, Adafruit, AliExpress. Look for industrial-grade supplies rated for LED matrices.

### Assembly

Wire the HUB75 matrix to the ESP32-S3 according to the pinout in `firmware/DeskMatrix/config.h` (GPIO pins used for the DMA display driver). Connect 5V power to the matrix's +5V and GND pins.

Optionally add the IMU (I²C: GPIO 47 SDA, GPIO 48 SCL) for tilt gestures.

## Project Structure

```
firmware/DeskMatrix/
├── DeskMatrix.ino          # Main loop, screen state machine
├── config.h                # Hardware config, feature flags
├── secrets.h                # (git-ignored) HTTP auth credentials
├── ConfigModel.*            # Config struct and serialization
├── SettingsStore.*          # Flash persistence (LittleFS)
├── ScreenStateMachine.h     # State machine for screen modes
├── services/
│   └── (headers) SpotifyService, ImuHardware
├── SpotifyService.cpp, ImuHardware.cpp   # Implementations
├── screens/
│   ├── (headers) ClockScreen, SpotifyScreen, ScreensaverScreen, DndScreen, BrbScreen
│   └── clockfaces/          # Mario, Words, Pacman, Canvas (JSON) presets — headers + assets
├── *Screen.cpp, *Clockface.cpp   # Implementations (Arduino compiles .cpp anywhere under the sketch root, so these currently sit flat next to DeskMatrix.ino while their headers are organized by feature)
├── web/
│   └── ConfigServer.h       # HTTP server + REST API (implementation: ConfigServer.cpp)
└── lib/                     # Vendored clockface engine (cw-gfx-engine, cw-commons)
```

## Getting Started

**Quick setup:** See [SETUP.md](SETUP.md) for two paths — download a pre-built release, or build from source with library installation and credentials setup.

### 1. Build Prerequisites

- **Arduino IDE 2.0+** or **Arduino CLI**
- **ESP32 Board Support** (via Boards Manager, `esp32:esp32`)
- **Required Libraries** (via Library Manager, exact registry names):
  - `WiFiManager`
  - `ArduinoJson`
  - `PNGdec`
  - `AnimatedGIF`
  - `JPEGDEC`
  - `Adafruit GFX Library`
  - `Adafruit BusIO`
  - `ESP32 HUB75 LED MATRIX PANEL DMA Display`
  - `SensorLib` (QMI8658 IMU driver)
- **SpotifyArduino** — not in the Library Manager registry; clone manually:
  ```bash
  git clone https://github.com/witnessmenow/spotify-api-arduino.git \
    "$HOME/Arduino/libraries/SpotifyArduino"
  ```

### 2. Configure Credentials

Copy the example secrets file and add your own credentials:

```bash
cp firmware/DeskMatrix/secrets.h.example firmware/DeskMatrix/secrets.h
```

Edit `secrets.h` and set a strong password for web access:

```cpp
#define CONFIG_AUTH_USER "admin"
#define CONFIG_AUTH_PASS "your_secure_password_here"
```

**Important:** Never commit `secrets.h` to git. It's in `.gitignore` for your safety.

### 3. Build & Upload

Using Arduino CLI:

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc \
  --build-property "build.partitions=large_littlefs_32MB" \
  firmware/DeskMatrix/DeskMatrix.ino

arduino-cli upload \
  --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc \
  --port /dev/cu.usbmodem* \
  firmware/DeskMatrix/DeskMatrix.ino
```

> The explicit `build.partitions` override works around a known ordering
> issue in esp32 core 3.0.0's `boards.txt` where the `FlashSize` menu's own
> default can otherwise silently win over the `PartitionScheme` selection.

Using Arduino IDE:
1. Open `firmware/DeskMatrix/DeskMatrix.ino`
2. Select Board: **ESP32S3 Dev Module**
3. Set partition scheme: **32MB Flash (4.8MB APP/22MB LittleFS)**
4. Click **Upload**

Or skip building entirely and grab a pre-built `.bin` from
[Releases](../../releases) — see [docs/BUILDING.md](docs/BUILDING.md).

### 4. First Boot

The device will:
1. Show its IP address on the matrix for 10 seconds
2. Enter WiFi provisioning mode if unconfigured (connect to the `DeskMatrix-Setup` access point)
3. Open the config page at `http://<device-ip>/`
   - Username: `admin`
   - Password: (what you set in `secrets.h`)

## Configuration

Access the web UI at `http://<device-ip>/` (Basic Auth required).

### Settings

- **Brightness** — LED panel brightness
- **Timezone** — UTC offset for clock display
- **Spotify** — Client ID, secret, refresh token, poll interval
- **Clock Face** — Mario / Words / Pacman / Nyan Cat / Star Wars / Canvas (custom JSON)
- **Idle Mode** — Auto (clock, interrupted by the GIF screensaver every 5 min) / Always Clock / Always GIF
- **DND/BRB Art** — Custom GIF images for status screens
- **Firmware Update** — Upload a new `.bin` over WiFi (OTA)

### API Endpoints

| Method | Endpoint | Description |
|--------|----------|--------------|
| GET | `/` | Config page (HTML) |
| GET | `/api/config` | Current config (JSON) |
| PUT | `/api/config` | Update config (includes `clockFace`, `canvasJson`, Spotify credentials, etc.) |
| POST | `/api/wifi` | Set WiFi credentials |
| GET | `/api/wifi/scan` | Scan nearby networks |
| POST | `/api/wifi/forget` | Erase WiFi credentials and reboot into setup mode |
| POST | `/api/assets` | Upload a named binary asset (`?id=...`) |
| POST | `/api/screensaver` | Upload the screensaver GIF |
| POST | `/api/dnd` | Upload the DND/BRB GIF |
| POST | `/api/ota` | Upload new firmware `.bin` (OTA update, device restarts on success) |
| POST | `/api/shutdown` | Graceful shutdown |

All POST/PUT endpoints require Basic Auth.

## Features in Detail

### Spotify Integration

Requires:
1. [Spotify Developer Account](https://developer.spotify.com/dashboard)
2. Create an app to get Client ID and Secret
3. Authorize the app once to get a refresh token
4. Paste credentials into the config page

The device polls Spotify (configurable interval, default 5s, with exponential backoff up to 2 minutes on repeated failures) and displays:
- Now playing track info
- Album art as a rotating vinyl record
- Artist and album name

**Note:** Album art fetches use `setInsecure()` (certificate verification disabled). Traffic remains encrypted; server identity verification is skipped as an accepted trade-off.

### Canvas Clockface (Custom JSON Themes)

Select **Canvas (custom)** as the clock face, then paste a JSON theme into the config page's textarea. Compatible with themes from the [jnthas/clock-club](https://github.com/jnthas/clock-club) gallery. Two presets — **Nyan Cat** and **Star Wars** — are built in and selectable directly from the Clock Face dropdown without pasting anything.

Example theme structure:

```json
{
  "name": "My Theme",
  "version": "1.0",
  "author": "You",
  "bgColor": 0,
  "delay": 50,
  "setup": [
    { "type": "text", "content": "Hello", "x": 10, "y": 10, "fgColor": 65535 },
    { "type": "rect", "x": 0, "y": 0, "width": 64, "height": 10, "color": 255 }
  ],
  "sprites": [],
  "loop": []
}
```

Supported elements:
- `text` — Static text (uses clock fonts)
- `datetime` — Dynamic time/date (ezTime format strings: `H:i`, `j M Y`, etc.)
- `rect` / `fillrect` / `line` — Shapes
- `image` — Base64-encoded PNG sprite
- `sprite` — Animated sprite reference

### Firmware Updates (OTA)

Update firmware over WiFi without a USB cable:

- **Web UI**: config page → Device → Firmware Update → select a `.bin` file → Upload. Progress bar shows upload status; the device restarts automatically on success.
- **curl**: `curl -u admin:PASSWORD --data-binary @DeskMatrix.ino.bin http://<device-ip>/api/ota`

See [docs/BUILDING.md](docs/BUILDING.md) for the full flashing workflow, including downloading pre-built binaries from [Releases](../../releases).

## Continuous Integration

Every push to `main` that touches `firmware/**` triggers a GitHub Actions build ([.github/workflows/build.yml](.github/workflows/build.yml)):
1. Installs the ESP32 core and all required libraries
2. Compiles the firmware
3. Uploads the `.bin` as a build artifact
4. Creates a [GitHub Release](../../releases) with the binary and flashing instructions

Release binaries are built with an admin password from the `DEVICE_ADMIN_USER`/`DEVICE_ADMIN_PASS` repository secrets (maintainer-only), rather than the placeholder in `secrets.h.example` — so a public pre-built binary isn't protected by a publicly-known password. If you fork this repo, set your own values for those secrets, or just build from source with your own `secrets.h`.

## Memory Management

### RAM Layout

- **Internal RAM:** ~327KB total
  - WiFiManager, config parsing, stack: ~150KB
  - Display double-buffer: ~8KB
  - Available for decoders: ~100KB
- **External PSRAM:** 16MB
  - Clock face pixel buffers (~8KB)
  - GIF decoder + canvas (~32KB)
  - PNG decoder (~44KB)
  - JPEG decoder + album art (~25.5KB)
  - Theme JSON parsing (~variable)

### Optimization Notes

- All decoder instances (PNG/GIF/JPEG) are lazily allocated to PSRAM on first use
- Pixel buffers use macro wrappers to maintain API compatibility
- WiFiClientSecure TLS buffers (~16KB per connection) require internal RAM
- Spotify polling includes exponential backoff (5s → 2min) on repeated failures to prevent heap exhaustion

## Troubleshooting

### Device won't connect to WiFi

1. Hold BOOT button for 5+ seconds — triggers WiFi reset
2. Connect to captive portal `DeskMatrix-Setup`
3. Select your network and enter password
4. Device reboots and connects

### Spotify not showing

1. Check config page — verify credentials are set
2. Restart device (power cycle)
3. Check Serial monitor for errors: `[spotify] failed to refresh access token`
4. Verify refresh token is still valid (Spotify revokes tokens after ~1 year of non-use)

### Web UI timing out

1. Try accessing `/api/config` directly (smaller response)
2. If API works but HTML hangs, clear browser cache
3. Restart device and try again

### Memory errors / device restarting

1. Reduce Spotify poll interval (config page)
2. Disable custom image elements in themes
3. Use smaller GIF files (~100KB max)

### Firmware won't flash via OTA

1. Try USB flash instead
2. Check device is connected to WiFi: `ping <device-ip>`
3. Verify credentials are correct

## Development

### Running Tests

```bash
cd tools
python3 -m pytest
```

### Adding a New Clock Face

1. Create `firmware/DeskMatrix/screens/clockfaces/yourtheme/` for the header/assets
2. Implement the `IClockface` interface (see `firmware/DeskMatrix/lib/cw-commons/IClockface.h`)
3. Add the `.cpp` implementation alongside `DeskMatrix.ino` (Arduino's sketch build only auto-compiles `.cpp` files at the sketch root, not arbitrary subfolders)
4. Wire it into `ClockScreen.cpp`'s `loadClockFace()` and the config page dropdown in `ConfigServer.cpp`

### Modifying the Config Schema

1. Update `ConfigModel.h` struct
2. Update `parseConfig()` in `ConfigModel.cpp`
3. Update `serializeConfig()` in `ConfigModel.cpp`
4. Update config page HTML in `ConfigServer.cpp`

## Credits & Inspiration

This project builds on the excellent work of:

- **[Clockwise](https://github.com/jnthas/clockwise)** by jnthas — The animated clock face engine and preset themes (Mario, Words, Pacman, etc.) are ported from Clockwise's modular clockface system. The Canvas JSON theme engine and rendering pipeline are inspired by Clockwise's architecture, and Canvas themes are compatible with the [jnthas/clock-club](https://github.com/jnthas/clock-club) community theme gallery.

- **[Spotify Matrix](https://github.com/tnarla/spotify-matrix)** by tnarla — The Spotify integration layout, album art display, and spinning vinyl record animation are adapted from this project's implementation.

- **[spotify-api-arduino](https://github.com/witnessmenow/spotify-api-arduino)** by Brian Lough (witnessmenow) — The underlying Spotify Web API client library.

- **[ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA)** — The DMA-based display driver that makes high-refresh-rate animation possible.

## License

MIT License

## Contributing

This is a personal project. Feel free to fork and adapt for your own desk display! Contributions welcome.

---

**Built with:** Arduino, ESP32, HUB75 matrix panel, and a lot of late-night debugging.

**Have questions?** Check the [GitHub Issues](https://github.com/valaypatel/DeskMatrix-firmware/issues) or review the firmware source comments.
