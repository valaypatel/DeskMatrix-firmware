# DeskMatrix Firmware Setup

## Before Building

You need to set up external libraries that aren't available in the Arduino Library Manager:

```bash
# From the repo root:
mkdir -p firmware/DeskMatrix/lib

# ESP32-HUB75-MatrixPanel-I2S-DMA (display driver)
git clone https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA.git \
  firmware/DeskMatrix/lib/ESP32-HUB75-MatrixPanel-I2S-DMA

# SensorQMI8658 (IMU/tilt sensor - optional)
git clone https://github.com/haechi/QMI8658.git \
  firmware/DeskMatrix/lib/QMI8658

# SpotifyArduino (Spotify integration)
# This library may need to be added manually or is available via:
# https://github.com/MartinMueller2003/SpotifyArduino
```

Then follow the build instructions in `BUILDING.md` or `README.md`.

## Copy Secrets

```bash
cp firmware/DeskMatrix/secrets.h.example firmware/DeskMatrix/secrets.h
# Edit secrets.h with your admin password
```

You're ready to build!
