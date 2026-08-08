// firmware/DeskMatrix/config.h
#pragma once

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define BOOT_BUTTON_PIN 0
#define WIFI_RESET_HOLD_MS 5000
#define IP_DISPLAY_SECONDS 10

// MVP simplification: fixed UTC offset rather than full timezone database.
// Change to your local offset in seconds (e.g. -18000 for US Eastern).
#define TIMEZONE_OFFSET_SEC 0
