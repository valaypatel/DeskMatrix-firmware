# DeskMatrix Firmware Setup

## Option A: Download a Pre-built Release (fastest)

Every push to `main` is automatically compiled by GitHub Actions. Grab the
latest `.bin` from [Releases](../../releases) and flash it — see
[docs/BUILDING.md](docs/BUILDING.md) for OTA / USB flashing instructions.
No local library setup needed for this path.

## Option B: Build From Source

### 1. Install Arduino Library Manager dependencies

Most dependencies are in the standard registry and install with one
command:

```bash
arduino-cli lib install \
  "WiFiManager" \
  "ArduinoJson" \
  "PNGdec" \
  "AnimatedGIF" \
  "JPEGDEC" \
  "Adafruit GFX Library" \
  "Adafruit BusIO" \
  "ESP32 HUB75 LED MATRIX PANEL DMA Display" \
  "SensorLib"
```

(In Arduino IDE: Library Manager → search and install each by name above.)

### 2. Install SpotifyArduino manually

This one isn't in the Library Manager registry — clone it directly into
your Arduino libraries folder:

```bash
git clone https://github.com/witnessmenow/spotify-api-arduino.git \
  "$HOME/Arduino/libraries/SpotifyArduino"
```

### 3. Copy secrets

```bash
cp firmware/DeskMatrix/secrets.h.example firmware/DeskMatrix/secrets.h
# Edit secrets.h and set your own CONFIG_AUTH_PASS
```

`secrets.h` is git-ignored — it will never be committed.

### 4. Build

See [README.md](README.md#3-build--upload) or [docs/BUILDING.md](docs/BUILDING.md)
for the full `arduino-cli compile`/`upload` commands and board settings.

You're ready to build!
