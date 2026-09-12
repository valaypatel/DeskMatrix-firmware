# DeskMatrix Firmware

An ESP32-S3 LED matrix display platform for your desk — animated clocks, Spotify now-playing, and customizable screensavers.

## Features

- **Animated Clock Faces** — Mario, Words, Pokedex themes with full-screen animation
- **GIF Screensaver** — Play GIFs from LittleFS with automatic looping and interleaving
- **Spotify Integration** — Display current track, artist, and album art in a spinning vinyl record
- **Custom JSON Themes** — Upload and render JSON-driven clock themes (text, shapes, datetime, sprites)
- **Web Configuration** — WiFi setup, settings management, and live theme uploads via HTTP
- **Do Not Disturb (DND)** — Physical tilt sensor to trigger DND/BRB status with custom GIFs
- **Responsive Config API** — Fully REST-compliant API with atomic settings updates

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
| 6-Axis IMU (MPU6050 or similar) | 1 | Optional — for tilt gesture detection |

### Power Supply Notes

The HUB75 matrix's power draw varies by content:
- **Idle/Clock**: 2-5A at 5V
- **Animated GIF**: 5-15A at 5V  
- **Full White Brightness**: 15-20A at 5V

**Recommended:** 5V 20A power supply with quality wiring and a capacitor across the supply leads to smooth inrush current.

Common sources: Amazon, Adafruit, AliExpress. Look for industrial-grade supplies rated for LED matrices.

### Assembly

Wire the HUB75 matrix to the ESP32-S3 according to the pinout in `firmware/DeskMatrix/config.h` (GPIO pins 2-18 used for the DMA display driver). Connect 5V power to the matrix's +5V and GND pins.

Optionally add the IMU (I²C: GPIO 41 SDA, GPIO 42 SCL) for tilt gestures.

## Project Structure

```
firmware/DeskMatrix/
├── DeskMatrix.ino          # Main loop, screen state machine
├── config.h                # Hardware config, feature flags
├── secrets.h               # (git-ignored) HTTP auth credentials
├── ConfigModel.*           # Config struct and serialization
├── SettingsStore.*         # Flash persistence (LittleFS)
├── ScreenStateMachine.*    # State machine for screen modes
├── services/
│   ├── SpotifyService.*    # Spotify API polling
│   ├── ImuHardware.*       # IMU tilt detection
│   └── WiFiManager         # WiFi provisioning
├── screens/
│   ├── ClockScreen.*       # Clock face selection & rendering
│   ├── SpotifyScreen.*     # Spinning vinyl record renderer
│   ├── ScreensaverScreen.* # GIF playback
│   ├── DndScreen.*         # DND indicator
│   ├── BrbScreen.*         # BRB indicator
│   └── clockfaces/         # Preset and custom clock themes
├── web/
│   ├── ConfigServer.*      # HTTP server + REST API
│   └── (HTML/JS config page)
└── lib/                    # External libraries + custom engines
```

## Getting Started

### 1. Build Prerequisites

- **Arduino IDE 2.0+** or **Arduino CLI**
- **ESP32 Board Support** (via Boards Manager)
- **Required Libraries** (via Library Manager):
  - `WiFiManager` (tzapu)
  - `ArduinoJson` (bblanchon)
  - `PNGdec` (bitbank2)
  - `AnimatedGIF` (bitbank2)
  - `JPEGDEC` (bitbank2)
  - `ESP32-HUB75-MatrixPanel-I2S-DMA` (mrfaptastic)
  - `SpotifyArduino` (MartinMueller2003)

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

**Note:** GitHub Actions CI builds are currently a work-in-progress (external libraries not auto-installing). Build locally using Arduino CLI or IDE instead.

Using Arduino CLI:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc firmware/DeskMatrix/DeskMatrix.ino

arduino-cli upload --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc --port /dev/cu.usbmodem* firmware/DeskMatrix/DeskMatrix.ino
```

Using Arduino IDE:
1. Open `firmware/DeskMatrix/DeskMatrix.ino`
2. Select Board: **ESP32S3 Dev Module**
3. Set partition scheme: **16M Flash (2MB APP / 12.5MB SPIFFS)** or **32MB (App 5MB + Little FS 24MB)**
4. Click **Upload**

### 4. First Boot

The device will:
1. Show its IP address on the matrix for 10 seconds
2. Enter WiFi provisioning mode if unconfigured
3. Open the config page at `http://<device-ip>/`
   - Username: `admin`
   - Password: (what you set in `secrets.h`)

## Configuration

Access the web UI at `http://<device-ip>/` (Basic Auth required).

### Settings

- **Brightness** — LED panel brightness (0–255)
- **Timezone** — UTC offset for clock display
- **Spotify** — Client ID, secret, refresh token, poll interval
- **Clock Face** — Mario / Words / Pokedex / Custom JSON theme
- **Idle Mode** — Auto (clock + GIF interlude) / Always Clock / Always GIF
- **DND/BRB Art** — Custom GIF images for status screens
- **Sleep Mode** — Turn off display at specific times

### API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Config page (HTML) |
| GET | `/api/config` | Current config (JSON) |
| PUT | `/api/config` | Update config |
| POST | `/api/upload/screensaver` | Upload screensaver GIF |
| POST | `/api/upload/dnd` | Upload DND GIF |
| POST | `/api/upload/brb` | Upload BRB GIF |
| POST | `/api/upload/theme` | Upload custom JSON theme |
| POST | `/api/shutdown` | Graceful shutdown |

All POST/PUT endpoints require Basic Auth.

## Features in Detail

### Spotify Integration

Requires:
1. [Spotify Developer Account](https://developer.spotify.com/dashboard)
2. Create an app to get Client ID and Secret
3. Authorize the app once to get a refresh token
4. Paste credentials into config page

The device polls Spotify every 10 seconds (configurable) and displays:
- Now playing track info
- Album art as a rotating vinyl record
- Artist and album name

**Note:** Album art fetches use `setInsecure()` (certificate verification disabled). Traffic remains encrypted; server identity verification is skipped as an accepted trade-off.

### Custom JSON Themes

Upload a JSON theme file via the config page. Example structure:

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

## Development

### Running Tests

```bash
cd tools
python3 -m pytest
```

### Adding a New Clock Face

1. Create `firmware/DeskMatrix/screens/clockfaces/yourtheme/`
2. Implement `IClockface` interface
3. Add to `ClockScreen.cpp` enum and factory
4. Update config page dropdown

### Modifying the Config Schema

1. Update `ConfigModel.h` struct
2. Update `parseConfig()` in `ConfigModel.cpp`
3. Update `serializeConfig()` in `ConfigModel.cpp`
4. Update config page HTML in `ConfigServer.cpp`

## Credits & Inspiration

This project builds on the excellent work of:

- **[Clockwise](https://github.com/jnthas/clockwise)** by jnthas — The animated clock face engine and preset themes (Mario, Words, Pokedex, Nyan Cat, etc.) are ported from Clockwise's modular clockface system. The JSON theme engine and rendering pipeline are inspired by Clockwise's architecture.

- **[Spotify Matrix](https://github.com/tnarla/spotify-matrix)** by tnarla — The Spotify integration, album art display, and spinning vinyl record animation are adapted from this project's elegant implementation.

- **[ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA)** — The DMA-based display driver that makes high-refresh-rate animation possible.

## License

MIT License

## Contributing

This is a personal project. Feel free to fork and adapt for your own desk display! Contributions welcome.

---

**Built with:** Arduino, ESP32, HUB75 matrix panel, and a lot of late-night debugging.

**Have questions?** Check the [GitHub Issues](https://github.com/valaypatel/DeskMatrix-firmware/issues) or review the firmware source comments.
