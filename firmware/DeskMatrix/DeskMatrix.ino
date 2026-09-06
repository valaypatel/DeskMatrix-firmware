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
#include "TiltDebouncer.h"
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
TiltDebouncer tiltDebouncer(25.0f, 5);
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

void showIpForSeconds(const String& ip, int seconds) {
  dma_display->clearScreen();
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setCursor(2, 2);
  dma_display->setTextColor(dma_display->color565(80, 200, 255));
  dma_display->print("IP:");
  dma_display->setCursor(2, 12);
  dma_display->print(ip);
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
  }

  spotifyService->loop(); // polls continuously regardless of current mode, so a mode switch happens promptly

  SpotifyStatus spotify = spotifyService->latest();
  if (spotify.isPlaying) {
    stateMachine.spotifyStarted();
  } else {
    stateMachine.spotifyStopped();
  }

  unsigned long nowMs = millis();

#if ENABLE_IMU_TILT
  static unsigned long lastImuPollMs = 0;
  if (imuAvailable && (nowMs - lastImuPollMs) >= 150) {
    lastImuPollMs = nowMs;
    float angle = imuReadTiltDegrees();
    TiltDirection dir = tiltDebouncer.update(angle);
    if (dir == TiltDirection::LEFT) stateMachine.tiltLeft();
    else if (dir == TiltDirection::RIGHT) stateMachine.tiltRight();
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
    // Not gated by the fixed tick below: drawSpotifyScreen() only redraws
    // (and flips) when the album art URL actually changes. With true
    // double-buffering, flipping on every tick regardless — as the block
    // below does for DND/BRB, which redraw unconditionally — would swap in
    // whatever stale/blank content is sitting in the other buffer,
    // producing a visible flash every second (confirmed on real hardware).
    drawSpotifyScreen(dma_display, spotify.albumArtUrl);
    return;
  }

  static unsigned long lastRenderMs = 0;
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    switch (stateMachine.mode()) {
      case ScreenMode::DND:
        drawDndScreen(dma_display);
        break;
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
