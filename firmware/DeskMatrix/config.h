// firmware/DeskMatrix/config.h
#pragma once

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define BOOT_BUTTON_PIN 0
#define WIFI_RESET_HOLD_MS 5000
#define IP_DISPLAY_SECONDS 10

// HTTP Basic Auth for the config web page and its API (GET /, /api/*).
// These credentials are defined in secrets.h (which is git-ignored).
// Copy secrets.h.example to secrets.h and customize before building.
#include "secrets.h"

// Timezone is now configurable live from the config page (AppConfig.
// timezoneOffsetMinutes, see ConfigModel.h) rather than a fixed build-time
// offset.

// Set to 1 once the axis is calibrated to re-enable tilt-triggered DND/BRB.
#define ENABLE_IMU_TILT 1
