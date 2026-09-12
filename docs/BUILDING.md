# Building DeskMatrix Firmware

There are three ways to build and flash the firmware:

## Option 1: GitHub Actions (Recommended for Quick Builds)

Every push to `main` automatically builds the firmware. Releases are created with `.bin` files ready to upload.

**Steps:**
1. Commit and push to GitHub
2. Go to [GitHub Actions](../../actions) tab
3. Find your build in "Build Firmware" workflow
4. Download the artifact `DeskMatrix-firmware` (contains `DeskMatrix.ino.bin`)
5. Upload via web UI or curl

**Flash from command line:**
```bash
curl -u admin:PASSWORD --data-binary @DeskMatrix.ino.bin http://<device-ip>/api/ota
```

**Flash from web UI:**
1. Go to `http://<device-ip>/` (requires Basic Auth)
2. Scroll to "Device" section
3. Click "Firmware Update"
4. Select the `.bin` file and upload
5. Device will restart automatically

## Option 2: Local Build with Arduino CLI

**Install Arduino CLI:**
```bash
# macOS
brew install arduino-cli

# Linux/Windows - see https://arduino.cc/pro/software-arduino-cli/
```

**Build:**
```bash
cd /path/to/RGBMatrix

# Install board support
arduino-cli core install esp32:esp32

# Install libraries
arduino-cli lib install "WiFiManager" "ArduinoJson" "PNGdec" "AnimatedGIF" "JPEGDEC" "SpotifyArduino"

# Set up credentials
cp firmware/DeskMatrix/secrets.h.example firmware/DeskMatrix/secrets.h
# Edit secrets.h with your admin password

# Compile
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc \
  --output-dir build \
  firmware/DeskMatrix/DeskMatrix.ino

# Firmware is at: build/DeskMatrix.ino.bin
```

**Flash via USB (first time or if WiFi is broken):**
```bash
# Find your device (looks like /dev/cu.usbmodem* or /dev/ttyACM*)
ls /dev/cu.usbmodem* 2>/dev/null || ls /dev/ttyACM*

# Flash
arduino-cli upload \
  --fqbn esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi,PartitionScheme=app5M_little24M_32MB,CDCOnBoot=cdc \
  --port /dev/cu.usbmodem* \
  firmware/DeskMatrix/DeskMatrix.ino
```

**Flash via OTA (WiFi):**
```bash
curl -u admin:PASSWORD --data-binary @build/DeskMatrix.ino.bin http://<device-ip>/api/ota
```

## Option 3: Arduino IDE 2.0+

1. Open Arduino IDE 2.0 or later
2. Install ESP32 board support: Boards Manager → search "esp32" → install by Espressif
3. Install libraries: Library Manager → search and install:
   - WiFiManager
   - ArduinoJson
   - PNGdec
   - AnimatedGIF
   - JPEGDEC
   - SpotifyArduino
4. Copy `firmware/DeskMatrix/secrets.h.example` to `firmware/DeskMatrix/secrets.h`
5. Edit `secrets.h` with your admin password
6. Open `firmware/DeskMatrix/DeskMatrix.ino` in Arduino IDE
7. Select Board: **ESP32S3 Dev Module**
8. Set partition scheme: **16M Flash (2MB APP / 12.5MB SPIFFS)**
9. Click **Compile** (check mark icon)
10. If building for the first time via USB, click **Upload** (arrow icon)

## Troubleshooting

**"Cannot find arduino-cli"**
- Ensure Arduino CLI is in your PATH
- Reinstall: `brew reinstall arduino-cli` (macOS)

**"Board not found esp32:esp32:esp32s3"**
- Install board support: `arduino-cli core install esp32:esp32`

**"Library not found: WiFiManager"**
- Install libraries: `arduino-cli lib install "WiFiManager"`

**"secrets.h not found"**
- Copy the example: `cp firmware/DeskMatrix/secrets.h.example firmware/DeskMatrix/secrets.h`

**Firmware won't flash via OTA**
- Try USB flash instead (Step 6 above)
- Check device is connected to WiFi: `ping <device-ip>`
- Verify credentials are correct in the curl command

**Device stuck in boot loop after OTA**
- Hold BOOT button for 5 seconds to trigger WiFi reset
- Flash stable firmware via USB

## Board Settings

The correct board configuration for this project:

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| USB Mode | CDC On Boot |
| Flash Size | 32MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (app 5MB + SPIFFS 24MB) |

These settings are required for the 16MB PSRAM and LittleFS support.

## GitHub Actions Builds

Every push to `main` triggers an automatic build. Releases are created with the firmware binary.

**Status:** Check the [Actions tab](../../actions)

**Download firmware:**
1. Go to a build run
2. Scroll to "Artifacts"
3. Download `DeskMatrix-firmware`
4. Extract `DeskMatrix.ino.bin`

**Releases:**
- Automatic releases are created for each build on `main`
- Named `firmware-<build-number>`
- Includes download links and flash instructions
