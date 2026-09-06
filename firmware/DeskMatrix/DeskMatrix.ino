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
bool tapDetectAvailable = false;
bool screenOff = false; // toggled by double-tap, independent of ScreenMode
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
  if (!imuAvailable) {
    Serial.println("IMU not found — DND/BRB disabled this boot.");
  } else {
    tapDetectAvailable = imuEnableTap();
    if (!tapDetectAvailable) Serial.println("IMU tap detection failed to configure.");
  }

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

  // Double-tap toggles the screen on/off, independent of ENABLE_IMU_TILT
  // and ScreenMode — a tap doesn't have tilt's "misreads level as tilted"
  // calibration problem (it's edge-triggered on a real physical event, not
  // a continuous ambiguous angle), so this stays enabled even while tilt
  // is off. Polled every iteration (not gated, unlike the tilt poll above)
  // — see imuCheckDoubleTap()'s comment: a tap is a brief transient that a
  // coarser interval could miss, and there's no interrupt pin wired here.
  if (tapDetectAvailable) {
    if (imuCheckDoubleTap()) {
      screenOff = !screenOff;
      Serial.println(screenOff ? "[tap] double-tap: screen off" : "[tap] double-tap: screen on");
      if (screenOff) {
        // Clear+flip twice: double-buffered, so a single pass only blanks
        // one of the two physical buffers — the next flip from whatever
        // resumes later would flash the other buffer's stale content.
        dma_display->clearScreen();
        dma_display->flipDMABuffer();
        dma_display->clearScreen();
        dma_display->flipDMABuffer();
      }
    }
  }
  if (screenOff) return; // config/Spotify polling above keeps running; only rendering pauses

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
  // settling into the normal DND indicator.
  static ScreenMode lastMode = ScreenMode::WIFI_SETUP;
  static unsigned long dndEnteredMs = 0;
  if (stateMachine.mode() == ScreenMode::DND && lastMode != ScreenMode::DND) {
    dndEnteredMs = nowMs;
  }
  lastMode = stateMachine.mode();

  static unsigned long lastRenderMs = 0;
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    switch (stateMachine.mode()) {
      case ScreenMode::DND:
        if ((nowMs - dndEnteredMs) < (unsigned long)IP_DISPLAY_SECONDS * 1000UL) {
          drawIpScreen(WiFi.localIP().toString());
        } else {
          drawDndScreen(dma_display);
        }
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
