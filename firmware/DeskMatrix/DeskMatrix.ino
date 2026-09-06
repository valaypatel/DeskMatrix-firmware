// firmware/DeskMatrix/DeskMatrix.ino
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFiManager.h>
#include "config.h"
#include "ConfigModel.h"
#include "SettingsStore.h"
#include "screens/ScreensaverScreen.h"
#include "services/SpotifyService.h"
#include "screens/SpotifyScreen.h"
#include "ScreenStateMachine.h"
#include "OrientationDebouncer.h"
#include "services/ImuHardware.h"
#include "screens/DndScreen.h"
#include "screens/BrbScreen.h"
#include "web/ConfigServer.h"

MatrixPanel_I2S_DMA *dma_display = nullptr;
WiFiManager wm;

SettingsStore settingsStore;
AppConfig appConfig;
ConfigServer configServer(appConfig, settingsStore);
ScreenStateMachine stateMachine;
OrientationDebouncer orientationDebouncer(5);
bool imuAvailable = false;
SpotifyService* spotifyService = nullptr;

AppConfig defaultConfig() {
  AppConfig cfg;
  cfg.spotify = SpotifyConfig{}; // empty credentials: wired up in Task 9b
  cfg.dndArt = "dnd_default";
  cfg.brbArt = "brb_default";
  return cfg;
}

void loadOrInitConfig() {
  std::string json;
  if (settingsStore.load(json)) {
    std::string error;
    if (parseConfig(json, appConfig, error)) {
      Serial.println("Loaded config from flash.");
      return;
    }
    Serial.print("Stored config invalid, using default: ");
    Serial.println(error.c_str());
  } else {
    Serial.println("No stored config found, using default.");
  }
  appConfig = defaultConfig();
  settingsStore.save(serializeConfig(appConfig));
}

void initPanel() {
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e = 9;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.double_buff = true; // avoid tearing/flicker by drawing to a back buffer and flipping once per frame

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();
}

void drawIpScreen(const String& ip) {
  dma_display->clearScreen();
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setCursor(2, 2);
  dma_display->setTextColor(dma_display->color565(80, 200, 255));
  dma_display->print("IP:");
  dma_display->setCursor(2, 12);
  dma_display->print(ip);
}

void showIpForSeconds(const String& ip, int seconds) {
  drawIpScreen(ip);
  delay((unsigned long)seconds * 1000UL);
  dma_display->clearScreen();
}

void connectWifi() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  wm.setConfigPortalTimeout(180);

  Serial.println("Starting Wi-Fi...");
  bool connected = wm.autoConnect("DeskMatrix-Setup");

  if (!connected) {
    Serial.println("Wi-Fi setup timed out. Restarting...");
    delay(2000);
    ESP.restart();
  }

  Serial.print("Wi-Fi connected. IP: ");
  Serial.println(WiFi.localIP());
  showIpForSeconds(WiFi.localIP().toString(), IP_DISPLAY_SECONDS);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  initPanel();
  connectWifi();
  settingsStore.begin();
  loadOrInitConfig();
  dma_display->setBrightness8(appConfig.brightness); // initPanel()'s 90 was just a pre-config-load placeholder

  loadScreensaverGif(); // ok if this returns false: screensaver just shows blank until one is uploaded
  spotifyService = new SpotifyService(appConfig.spotify.clientId, appConfig.spotify.clientSecret,
                                       appConfig.spotify.refreshToken, appConfig.spotify.pollSec);

  imuAvailable = imuBegin();
  if (!imuAvailable) Serial.println("IMU not found — DND/BRB disabled this boot.");

  stateMachine.wifiConfigured(); // Wi-Fi already connected above; move state machine to SCREENSAVER

  configServer.begin();
  Serial.print("Config API ready at http://");
  Serial.println(WiFi.localIP());

  configTime(TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org");
}

void loop() {
  configServer.loop();

  if (configServer.configChanged()) {
    Serial.println("[config] change detected");
    spotifyService->configure(appConfig.spotify.clientId, appConfig.spotify.clientSecret,
                               appConfig.spotify.refreshToken, appConfig.spotify.pollSec);
    dma_display->setBrightness8(appConfig.brightness);
  }

  spotifyService->loop(); // polls continuously regardless of current mode, so a mode switch happens promptly

  SpotifyStatus spotify = spotifyService->latest();
  if (spotify.isPlaying) {
    stateMachine.spotifyStarted();
  } else {
    stateMachine.spotifyStopped();
  }

  unsigned long nowMs = millis();

  // Hold the physical BOOT button for WIFI_RESET_HOLD_MS to forget the
  // saved Wi-Fi network and reboot into the "DeskMatrix-Setup" captive
  // portal — the way to recover if the device is moved somewhere its
  // current network doesn't reach and there's no other way to reach the
  // config API. Checked every iteration, before any early-return below, so
  // it works regardless of current ScreenMode. BOOT_BUTTON_PIN is
  // INPUT_PULLUP, so pressed reads LOW.
  static unsigned long buttonHeldSinceMs = 0;
  static bool wifiResetTriggered = false;
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (buttonHeldSinceMs == 0) buttonHeldSinceMs = nowMs;
    if (!wifiResetTriggered && (nowMs - buttonHeldSinceMs) >= WIFI_RESET_HOLD_MS) {
      wifiResetTriggered = true;
      Serial.println("[wifi] BOOT held: forgetting Wi-Fi and restarting");
      WiFi.disconnect(true, true); // erase stored credentials from flash
      delay(200);
      ESP.restart();
    }
  } else {
    buttonHeldSinceMs = 0;
    wifiResetTriggered = false;
  }

#if ENABLE_IMU_TILT
  // "Tilt" here means physically rotating the whole panel 90° in place on
  // its desk stand (see services/ImuHardware.h) — tiltLeft()/tiltRight()/
  // tiltCenter() are just ScreenStateMachine's existing DND/BRB/normal API
  // reused for that gesture, not a rocking motion.
  static unsigned long lastImuPollMs = 0;
  if (imuAvailable && (nowMs - lastImuPollMs) >= 150) {
    lastImuPollMs = nowMs;
    PanelOrientation orientation = orientationDebouncer.update(imuReadOrientation());
    if (orientation == PanelOrientation::DND) stateMachine.tiltLeft();
    else if (orientation == PanelOrientation::BRB) stateMachine.tiltRight();
    else stateMachine.tiltCenter();
  }
#endif

  if (stateMachine.mode() == ScreenMode::SCREENSAVER) {
    // Not gated by any fixed tick: GIF playback paces itself off each
    // frame's own display duration (see ScreensaverScreen.cpp).
    drawScreensaverFrame(dma_display);
    return;
  }

  if (stateMachine.mode() == ScreenMode::SPOTIFY_PLAYING) {
    // Not gated by the fixed tick below, same reasoning as SCREENSAVER:
    // the spinning-record animation paces and flips itself every ~100ms
    // while playing (see SpotifyScreen.cpp) — folding it into the shared
    // 1s tick either flashes (flip without redraw) or spins too slowly.
    drawSpotifyScreen(dma_display, spotify.albumArtUrl, spotify.isPlaying);
    return;
  }

  // Tracks how long we've been in DND, so a fresh tilt-left shows the IP
  // first (a quick, no-reboot way to look up the device's address) before
  // settling into the normal DND indicator. A brief flicker back out of
  // DND (e.g. hand tremor while holding the panel, a tiny knock — the
  // orientation classifier debounces but can't fully rule this out) should
  // NOT restart the countdown, or it can look permanently "stuck" on the
  // IP screen if that happens repeatedly near the end of the wait
  // (confirmed live: exactly this symptom). Only treat DND as a genuinely
  // fresh session — and reset the countdown — if we've been out of it for
  // longer than a brief flicker plausibly lasts.
  constexpr unsigned long kDndSessionGraceMs = 2000;
  static ScreenMode lastMode = ScreenMode::WIFI_SETUP;
  static unsigned long dndEnteredMs = 0;
  static unsigned long lastDndExitMs = 0;
  if (stateMachine.mode() == ScreenMode::DND) {
    if (lastMode != ScreenMode::DND && (nowMs - lastDndExitMs) > kDndSessionGraceMs) {
      dndEnteredMs = nowMs;
    }
  } else if (lastMode == ScreenMode::DND) {
    lastDndExitMs = nowMs;
  }
  lastMode = stateMachine.mode();

  if (stateMachine.mode() == ScreenMode::DND) {
    if ((nowMs - dndEnteredMs) < (unsigned long)IP_DISPLAY_SECONDS * 1000UL) {
      // Static content: no need for a fixed tick, just avoid redundant
      // redraws every single loop() iteration.
      static unsigned long lastIpRenderMs = 0;
      if ((nowMs - lastIpRenderMs) >= 1000) {
        lastIpRenderMs = nowMs;
        drawIpScreen(WiFi.localIP().toString());
        dma_display->flipDMABuffer();
      }
    } else if (dndGifExists()) {
      // Not gated by a fixed tick: the GIF paces and flips itself (see
      // ScreensaverScreen.cpp), same reasoning as SCREENSAVER above.
      drawDndGifFrame(dma_display);
    } else {
      static unsigned long lastDndRenderMs = 0;
      if ((nowMs - lastDndRenderMs) >= 1000) {
        lastDndRenderMs = nowMs;
        drawDndScreen(dma_display);
        dma_display->flipDMABuffer();
      }
    }
    return;
  }

  static unsigned long lastRenderMs = 0;
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    switch (stateMachine.mode()) {
      case ScreenMode::BRB:
        drawBrbScreen(dma_display);
        break;
      case ScreenMode::INTERRUPT_TAKEOVER:
        // Scaffolded for future one-shot event pages — unused this phase.
        break;
      default:
        break;
    }
    dma_display->flipDMABuffer();
  }
}
