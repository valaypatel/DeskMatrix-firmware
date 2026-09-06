// firmware/DeskMatrix/config.h
#pragma once

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define BOOT_BUTTON_PIN 0
#define WIFI_RESET_HOLD_MS 5000
#define IP_DISPLAY_SECONDS 10

// HTTP Basic Auth for the config web page and its API (GET /, /api/*).
// No username was specified when this was requested, so "admin" was chosen.
#define CONFIG_AUTH_USER "admin"
#define CONFIG_AUTH_PASS "CHANGE_ME_IN_SECRETS_H"

// MVP simplification: fixed UTC offset rather than full timezone database.
// Change to your local offset in seconds (e.g. -18000 for US Eastern).
#define TIMEZONE_OFFSET_SEC 0

// Temporarily disabled: the IMU tilt axis/sign has not been calibrated
// against real hardware yet (see the comment in ImuHardware.cpp), and on
// first boot it was misreading level as tilted, getting stuck in DND/BRB.
// Set to 1 once the axis is calibrated to re-enable tilt-triggered DND/BRB.
#define ENABLE_IMU_TILT 0
