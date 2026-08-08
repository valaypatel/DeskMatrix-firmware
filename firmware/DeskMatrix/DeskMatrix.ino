// firmware/DeskMatrix/DeskMatrix.ino
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFiManager.h>
#include "config.h"
#include "ConfigModel.h"
#include "SettingsStore.h"
#include "WidgetRegistry.h"
#include "screens/ClockWidget.h"
#include "services/WeatherService.h"
#include "services/RemoteClockService.h"
#include "screens/WeatherWidget.h"
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
WidgetRegistry widgetRegistry;
WeatherService* weatherService = nullptr;
RemoteClockService remoteClockService(51.5074f, -0.1278f, 6UL * 3600UL * 1000UL); // London, poll every 6h
ScreenStateMachine stateMachine;
TiltDebouncer tiltDebouncer(25.0f, 5);
bool imuAvailable = false;

// Finds the weather widget's data source (preferring its `dataSource` id
// field, falling back to a type-based scan if no widget/match is found) and
// fills lat/lon/pollSec from it. Returns true if a weather data source was
// found at all.
bool findWeatherConfig(float& lat, float& lon, int& pollSec) {
  lat = 0; lon = 0; pollSec = 600;

  const DataSourceConfig* match = nullptr;

  std::string weatherDataSourceId;
  for (const auto& w : appConfig.homeWidgets) {
    if (w.type == "weather") {
      weatherDataSourceId = w.dataSource;
      break;
    }
  }

  if (!weatherDataSourceId.empty()) {
    for (const auto& ds : appConfig.dataSources) {
      if (ds.id == weatherDataSourceId) {
        match = &ds;
        break;
      }
    }
  }

  if (!match) {
    for (const auto& ds : appConfig.dataSources) {
      if (ds.type == "weather") {
        match = &ds;
        break;
      }
    }
  }

  if (!match) return false;

  sscanf(match->location.c_str(), "%f,%f", &lat, &lon);
  pollSec = match->pollSec;
  return true;
}

void initWeatherService() {
  float lat, lon;
  int pollSec;
  findWeatherConfig(lat, lon, pollSec);
  weatherService = new WeatherService(lat, lon, pollSec);
}

AppConfig defaultConfig() {
  AppConfig cfg;

  WidgetConfig localClock;
  localClock.id = "w1"; localClock.type = "clock"; localClock.style = "digital_with_day";
  localClock.color = "#FFDE59"; localClock.icon = "flag_in";
  localClock.x = 0; localClock.y = 0; localClock.w = 32; localClock.h = 16;
  cfg.homeWidgets.push_back(localClock);

  WidgetConfig remoteClock;
  remoteClock.id = "w4"; remoteClock.type = "clock"; remoteClock.style = "digital";
  remoteClock.color = "#FFDE59"; remoteClock.icon = "flag_gb";
  remoteClock.location = "51.5074,-0.1278"; // London
  remoteClock.x = 0; remoteClock.y = 16; remoteClock.w = 32; remoteClock.h = 16;
  cfg.homeWidgets.push_back(remoteClock);

  WidgetConfig weather;
  weather.id = "w2"; weather.type = "weather"; weather.style = "icon_temp";
  weather.icon = "weather_auto"; weather.color = "#4C9AFF";
  weather.x = 32; weather.y = 0; weather.w = 32; weather.h = 32;
  weather.dataSource = "ds_weather";
  cfg.homeWidgets.push_back(weather);

  DataSourceConfig ds;
  ds.id = "ds_weather"; ds.type = "weather"; ds.pollSec = 600;
  ds.location = "40.7128,-74.0060"; // MVP default; overwritten by real config once pushed
  cfg.dataSources.push_back(ds);

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

  registerClockWidget(widgetRegistry, remoteClockService);
  initWeatherService();
  registerWeatherWidget(widgetRegistry, *weatherService);

  imuAvailable = imuBegin();
  if (!imuAvailable) Serial.println("IMU not found — DND/BRB disabled this boot.");

  stateMachine.wifiConfigured(); // Wi-Fi already connected above; move state machine to HOME

  configServer.begin();
  Serial.print("Config API ready at http://");
  Serial.println(WiFi.localIP());

  configTime(TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org");
}

void loop() {
  configServer.loop();

  if (configServer.configChanged()) {
    Serial.println("[config] change detected");
    float lat, lon;
    int pollSec;
    if (findWeatherConfig(lat, lon, pollSec)) {
      Serial.printf("[config] applying weather lat=%.4f lon=%.4f pollSec=%d\n", lat, lon, pollSec);
      weatherService->configure(lat, lon, pollSec);
    } else {
      Serial.println("[config] no weather data source found in updated config");
    }
  }

  weatherService->loop();
  remoteClockService.loop();

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

  static unsigned long lastRenderMs = 0;
  if ((nowMs - lastRenderMs) >= 1000) {
    lastRenderMs = nowMs;
    switch (stateMachine.mode()) {
      case ScreenMode::HOME:
        dma_display->clearScreen();
        for (const auto& w : appConfig.homeWidgets) {
          widgetRegistry.draw(w.type, RenderContext{dma_display, &w});
        }
        break;
      case ScreenMode::DND:
        drawDndScreen(dma_display);
        break;
      case ScreenMode::BRB:
        drawBrbScreen(dma_display);
        break;
      case ScreenMode::INTERRUPT_TAKEOVER:
        // Scaffolded for future data sources (e.g. Jira) — MVP's `screens`
        // list is empty, so this state is never entered yet.
        break;
      default:
        break;
    }
    dma_display->flipDMABuffer();
  }
}
