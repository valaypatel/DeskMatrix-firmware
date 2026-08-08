// Headless Wi-Fi provisioning test.
//
// First boot (or after a credential reset): the board opens its own
// access point named "DeskMatrix-Setup". Connect to it from a phone,
// a captive portal page should open automatically (or visit
// http://192.168.4.1). Pick your Wi-Fi network and enter the password.
//
// The credentials are saved to flash, the board reboots, and joins
// that Wi-Fi automatically on every future boot -- no re-entry needed.
//
// Hold the BOOT button for 5s at any time to erase saved credentials
// and fall back into setup mode again.

#include <WiFiManager.h>

#define BOOT_BUTTON_PIN 0
#define RESET_HOLD_MS 5000

WiFiManager wm;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  wm.setConfigPortalTimeout(180); // give up and reboot after 3 min unconfigured

  Serial.println("Starting Wi-Fi... will open 'DeskMatrix-Setup' portal if no saved credentials");

  bool connected = wm.autoConnect("DeskMatrix-Setup");

  if (!connected) {
    Serial.println("Failed to connect / setup timed out. Restarting...");
    delay(2000);
    ESP.restart();
  }

  Serial.println("Wi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Hold BOOT for 5s to clear saved Wi-Fi credentials and re-provision.
  static unsigned long pressStart = 0;

  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (pressStart == 0) pressStart = millis();
    if (millis() - pressStart > RESET_HOLD_MS) {
      Serial.println("Erasing Wi-Fi credentials, restarting into setup mode...");
      wm.resetSettings();
      delay(500);
      ESP.restart();
    }
  } else {
    pressStart = 0;
  }

  delay(50);
}
