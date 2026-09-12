// firmware/DeskMatrix/ConfigServer.cpp
#include "web/ConfigServer.h"
#include "config.h"
#include "GifValidation.h"
#include "screens/ScreensaverScreen.h"
#include <Update.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <cstring>

// The single WiFiManager instance owned by DeskMatrix.ino — reused here so
// /api/wifi/forget can call its resetSettings(), which is the officially
// correct way to fully forget a network (confirmed on real hardware that
// WiFi.disconnect(true, true) alone doesn't reliably stop WiFiManager's
// own autoConnect() from reconnecting anyway on the next boot).
extern WiFiManager wm;

namespace {
// Single-page config UI: Wi-Fi, screensaver GIF upload, brightness.
// Deliberately plain (no framework, minimal inline JS) — served straight
// from flash, no LittleFS asset needed.
const char kConfigPageHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DeskMatrix Config</title>
<style>
body{font-family:-apple-system,sans-serif;max-width:420px;margin:2em auto;padding:0 1em;color:#222}
h1{font-size:1.3em}
section{margin-bottom:2em;padding-bottom:1.5em;border-bottom:1px solid #ddd}
label{display:block;margin-bottom:.4em;font-weight:600}
input[type=text],input[type=password],input[type=file]{width:100%;padding:.5em;margin-bottom:.8em;box-sizing:border-box}
input[type=range]{width:100%}
button{padding:.6em 1.2em;border:0;background:#222;color:#fff;border-radius:4px;cursor:pointer}
button:hover{background:#444}
.status{margin-top:.6em;font-size:.9em}
.ok{color:#0a7a2f}.err{color:#b00020}
</style></head>
<body>
<h1>DeskMatrix Config</h1>

<section>
<h2>Wi-Fi</h2>
<button type="button" id="wifiScanBtn">Scan for networks</button>
<div class="status" id="wifiScanStatus"></div>
<select id="wifiNetworks" style="width:100%;padding:.5em;margin:.6em 0;box-sizing:border-box" hidden></select>
<form id="wifiForm">
<label>SSID</label><input type="text" id="wifiSsid" required>
<label>Password</label><input type="password" id="wifiPass">
<button type="submit">Connect</button>
<div class="status" id="wifiStatus"></div>
</form>
<button type="button" id="wifiForgetBtn" style="background:#b00020;margin-top:.8em">Forget this Wi-Fi network</button>
<div class="status" id="wifiForgetStatus"></div>
</section>

<section>
<h2>Screensaver GIF</h2>
<label>Upload a 64x64 GIF (replaces the current one)</label>
<input type="file" id="gifFile" accept=".gif">
<button data-url="/api/screensaver" data-input="gifFile" data-status="gifStatus">Upload</button>
<div class="status" id="gifStatus"></div>
</section>

<section>
<h2>DND Screen</h2>
<label>Upload a 64x64 GIF shown when tilted left (after the IP display)</label>
<input type="file" id="dndFile" accept=".gif">
<button data-url="/api/dnd" data-input="dndFile" data-status="dndStatus">Upload</button>
<div class="status" id="dndStatus"></div>
</section>

<section>
<h2>Spotify</h2>
<label><input type="checkbox" id="spotifyEnabled" style="width:auto;margin-right:.5em"> Enable Spotify now-playing takeover</label>
<div class="status" id="spotifyEnabledStatus"></div>
</section>

<section>
<h2>Clock face</h2>
<label>Idle-screen clock (shown between screensaver GIF interludes)</label>
<select id="clockFace" style="width:100%;padding:.5em;margin-bottom:.4em;box-sizing:border-box">
<option value="mario">Mario</option>
<option value="words">Words</option>
<option value="pacman">Pacman</option>
<option value="nyancat">Nyan Cat</option>
<option value="starwars">Star Wars</option>
<option value="canvas">Canvas (custom)</option>
</select>
<div class="status" id="clockFaceStatus"></div>
<div id="canvasJsonSection" style="display:none;margin-top:.8em">
<label>Custom theme JSON (paste a theme from github.com/jnthas/clock-club)</label>
<textarea id="canvasJson" rows="8" style="width:100%;padding:.5em;box-sizing:border-box;font-family:monospace;font-size:.8em"></textarea>
<button type="button" id="canvasJsonSaveBtn" style="margin-top:.5em">Save custom theme</button>
<div class="status" id="canvasJsonStatus"></div>
</div>
</section>

<section>
<h2>Idle screen</h2>
<label>Clock vs. screensaver GIF</label>
<select id="idleMode" style="width:100%;padding:.5em;margin-bottom:.4em;box-sizing:border-box">
<option value="auto">Auto (alternate: clock, then GIF every 5 min)</option>
<option value="clock">Always clock</option>
<option value="gif">Always GIF</option>
</select>
<div class="status" id="idleModeStatus"></div>
</section>

<section>
<h2>Timezone</h2>
<label>Used for the clock and Wi-Fi setup's time display</label>
<select id="timezone" style="width:100%;padding:.5em;margin-bottom:.4em;box-sizing:border-box">
<option value="-720">UTC-12:00</option>
<option value="-660">UTC-11:00</option>
<option value="-600">UTC-10:00</option>
<option value="-540">UTC-09:00</option>
<option value="-480">UTC-08:00 (Pacific)</option>
<option value="-420">UTC-07:00 (Mountain)</option>
<option value="-360">UTC-06:00 (Central)</option>
<option value="-300">UTC-05:00 (Eastern)</option>
<option value="-240">UTC-04:00</option>
<option value="-210">UTC-03:30</option>
<option value="-180">UTC-03:00</option>
<option value="-120">UTC-02:00</option>
<option value="-60">UTC-01:00</option>
<option value="0">UTC+00:00</option>
<option value="60">UTC+01:00 (Central Europe)</option>
<option value="120">UTC+02:00 (Eastern Europe)</option>
<option value="180">UTC+03:00</option>
<option value="210">UTC+03:30 (Iran)</option>
<option value="240">UTC+04:00</option>
<option value="270">UTC+04:30 (Afghanistan)</option>
<option value="300">UTC+05:00</option>
<option value="330">UTC+05:30 (India)</option>
<option value="345">UTC+05:45 (Nepal)</option>
<option value="360">UTC+06:00</option>
<option value="390">UTC+06:30 (Myanmar)</option>
<option value="420">UTC+07:00</option>
<option value="480">UTC+08:00 (China/Singapore)</option>
<option value="525">UTC+08:45</option>
<option value="540">UTC+09:00 (Japan/Korea)</option>
<option value="570">UTC+09:30</option>
<option value="600">UTC+10:00</option>
<option value="630">UTC+10:30</option>
<option value="660">UTC+11:00</option>
<option value="720">UTC+12:00</option>
<option value="765">UTC+12:45</option>
<option value="780">UTC+13:00</option>
<option value="840">UTC+14:00</option>
</select>
<div class="status" id="timezoneStatus"></div>
</section>

<section>
<h2>Brightness</h2>
<label>Panel brightness (<span id="brightnessVal">-</span> / 255)</label>
<input type="range" id="brightness" min="0" max="255">
<div class="status" id="brightnessStatus"></div>
</section>

<section>
<h2>Sleep</h2>
<label><input type="checkbox" id="sleepEnabled" style="width:auto;margin-right:.5em"> Screen off (Wi-Fi and Spotify polling stay on; auto-wakes on Spotify playback)</label>
<div class="status" id="sleepEnabledStatus"></div>
</section>

<section>
<h2>Device</h2>
<h3 style="margin-top:0;font-size:1em;color:#555">Firmware Update</h3>
<p style="margin:0 0 .8em;font-size:.9em;color:#555">Upload a new firmware .bin file to update over-the-air. Device will restart automatically.</p>
<input type="file" id="firmwareFile" accept=".bin" style="margin-bottom:.8em">
<div style="display:flex;gap:.5em">
<button type="button" id="firmwareUploadBtn">Upload Firmware</button>
<button type="button" id="firmwareCancelBtn" disabled style="background:#999">Cancel</button>
</div>
<div id="firmwareProgress" style="display:none;margin:.8em 0">
<div style="background:#eee;border-radius:4px;overflow:hidden;height:24px">
<div id="firmwareProgressBar" style="background:#222;height:100%;width:0;transition:width 0.3s;display:flex;align-items:center;justify-content:center;font-size:.8em;color:#fff">0%</div>
</div>
</div>
<div class="status" id="firmwareStatus"></div>

<h3 style="margin-top:1.5em;font-size:1em;color:#555">Shutdown</h3>
<p style="margin:0 0 .8em;font-size:.9em;color:#555">Before unplugging the device, use this to make sure nothing is mid-write to flash.</p>
<button type="button" id="shutdownBtn" style="background:#b00020">Safe to unplug</button>
<div class="status" id="shutdownStatus"></div>
</section>

<script>
async function getConfig() { return (await fetch('/api/config')).json(); }
async function putConfig(cfg) {
  return fetch('/api/config', {method:'PUT', headers:{'Content-Type':'application/json'}, body: JSON.stringify(cfg)});
}

function updateCanvasSectionVisibility(clockFaceValue) {
  document.getElementById('canvasJsonSection').style.display = (clockFaceValue === 'canvas') ? 'block' : 'none';
}

getConfig().then(cfg => {
  document.getElementById('brightness').value = cfg.brightness;
  document.getElementById('brightnessVal').textContent = cfg.brightness;
  document.getElementById('clockFace').value = cfg.clockFace || 'mario';
  document.getElementById('timezone').value = cfg.timezoneOffsetMinutes || 0;
  document.getElementById('spotifyEnabled').checked = cfg.spotify ? cfg.spotify.enabled !== false : true;
  document.getElementById('sleepEnabled').checked = !!cfg.sleep;
  document.getElementById('canvasJson').value = cfg.canvasJson || '';
  updateCanvasSectionVisibility(cfg.clockFace || 'mario');
  document.getElementById('idleMode').value = cfg.idleMode || 'auto';
});

document.getElementById('idleMode').addEventListener('change', async e => {
  const status = document.getElementById('idleModeStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.idleMode = e.target.value;
    const res = await putConfig(cfg);
    status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
    status.className = res.ok ? 'status ok' : 'status err';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('sleepEnabled').addEventListener('change', async e => {
  const status = document.getElementById('sleepEnabledStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.sleep = e.target.checked;
    const res = await putConfig(cfg);
    status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
    status.className = res.ok ? 'status ok' : 'status err';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('spotifyEnabled').addEventListener('change', async e => {
  const status = document.getElementById('spotifyEnabledStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.spotify.enabled = e.target.checked;
    const res = await putConfig(cfg);
    status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
    status.className = res.ok ? 'status ok' : 'status err';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('clockFace').addEventListener('change', async e => {
  updateCanvasSectionVisibility(e.target.value);
  const status = document.getElementById('clockFaceStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.clockFace = e.target.value;
    const res = await putConfig(cfg);
    status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
    status.className = res.ok ? 'status ok' : 'status err';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('canvasJsonSaveBtn').addEventListener('click', async () => {
  const status = document.getElementById('canvasJsonStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.canvasJson = document.getElementById('canvasJson').value;
    const res = await putConfig(cfg);
    if (res.ok) {
      status.textContent = 'Saved.'; status.className = 'status ok';
    } else {
      const body = await res.json().catch(() => ({}));
      status.textContent = 'Failed: ' + (body.error || 'invalid JSON'); status.className = 'status err';
    }
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('timezone').addEventListener('change', async e => {
  const status = document.getElementById('timezoneStatus');
  status.textContent = 'Saving...'; status.className = 'status';
  try {
    const cfg = await getConfig();
    cfg.timezoneOffsetMinutes = parseInt(e.target.value, 10);
    const res = await putConfig(cfg);
    status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
    status.className = res.ok ? 'status ok' : 'status err';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('wifiScanBtn').addEventListener('click', async () => {
  const status = document.getElementById('wifiScanStatus');
  const select = document.getElementById('wifiNetworks');
  status.textContent = 'Scanning (a few seconds)...'; status.className = 'status';
  select.hidden = true;
  try {
    const res = await fetch('/api/wifi/scan');
    const body = await res.json();
    if (!res.ok) { status.textContent = 'Scan failed'; status.className = 'status err'; return; }
    const networks = body.networks || [];
    if (networks.length === 0) { status.textContent = 'No networks found.'; status.className = 'status'; return; }
    select.innerHTML = '<option value="">Select a network...</option>' + networks.map(n =>
      `<option value="${n.ssid}">${n.ssid} (${n.rssi} dBm${n.secure ? '' : ', open'})</option>`).join('');
    select.hidden = false;
    status.textContent = `Found ${networks.length} network(s).`; status.className = 'status ok';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('wifiNetworks').addEventListener('change', e => {
  if (e.target.value) document.getElementById('wifiSsid').value = e.target.value;
});

document.getElementById('wifiForm').addEventListener('submit', async e => {
  e.preventDefault();
  const status = document.getElementById('wifiStatus');
  status.textContent = 'Connecting...'; status.className = 'status';
  try {
    const res = await fetch('/api/wifi', {method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({ssid: document.getElementById('wifiSsid').value, password: document.getElementById('wifiPass').value})});
    const body = await res.json();
    if (res.ok) { status.textContent = 'Reconnecting to new network - this page will stop responding until it does.'; status.className = 'status ok'; }
    else { status.textContent = body.error || 'Failed'; status.className = 'status err'; }
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

document.getElementById('wifiForgetBtn').addEventListener('click', async () => {
  if (!confirm('Forget the saved Wi-Fi network and restart? The device will open its own "DeskMatrix-Setup" network for reconfiguring — this page will stop responding.')) return;
  const status = document.getElementById('wifiForgetStatus');
  status.textContent = 'Forgetting network...'; status.className = 'status';
  try {
    await fetch('/api/wifi/forget', {method:'POST'});
    status.textContent = 'Restarting - connect to the "DeskMatrix-Setup" Wi-Fi network to reconfigure.'; status.className = 'status ok';
  } catch (err) { status.textContent = 'Restarting (connection dropped as expected).'; status.className = 'status ok'; }
});

document.getElementById('shutdownBtn').addEventListener('click', async () => {
  if (!confirm('Halt the device for safe power-off? Everything stops responding until you unplug and power it back on.')) return;
  const status = document.getElementById('shutdownStatus');
  status.textContent = 'Halting...'; status.className = 'status';
  try {
    await fetch('/api/shutdown', {method:'POST'});
    status.textContent = 'Check the panel - once it shows "SAFE TO UNPLUG", it\'s safe to disconnect power.'; status.className = 'status ok';
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
});

async function uploadImage(url, inputId, statusId) {
  const status = document.getElementById(statusId);
  const file = document.getElementById(inputId).files[0];
  if (!file) { status.textContent = 'Choose a file first'; status.className = 'status err'; return; }
  status.textContent = 'Uploading...'; status.className = 'status';
  try {
    const res = await fetch(url, {method:'POST', body: file});
    const body = await res.json();
    if (res.ok) { status.textContent = 'Uploaded.'; status.className = 'status ok'; }
    else { status.textContent = body.error || 'Failed'; status.className = 'status err'; }
  } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
}
for (const btn of document.querySelectorAll('button[data-url]')) {
  btn.addEventListener('click', () => uploadImage(btn.dataset.url, btn.dataset.input, btn.dataset.status));
}

let brightnessTimer;
document.getElementById('brightness').addEventListener('input', e => {
  document.getElementById('brightnessVal').textContent = e.target.value;
  clearTimeout(brightnessTimer);
  brightnessTimer = setTimeout(async () => {
    const status = document.getElementById('brightnessStatus');
    try {
      const cfg = await getConfig();
      cfg.brightness = parseInt(e.target.value, 10);
      const res = await putConfig(cfg);
      status.textContent = res.ok ? 'Saved.' : 'Failed to save.';
      status.className = res.ok ? 'status ok' : 'status err';
    } catch (err) { status.textContent = 'Request failed: ' + err; status.className = 'status err'; }
  }, 300);
});

let firmwareUploadActive = false;
document.getElementById('firmwareUploadBtn').addEventListener('click', async () => {
  const file = document.getElementById('firmwareFile').files[0];
  if (!file) { alert('Choose a .bin file first'); return; }
  if (!confirm('Upload new firmware? Device will restart automatically. Do not power off during upload.')) return;

  firmwareUploadActive = true;
  const status = document.getElementById('firmwareStatus');
  const progress = document.getElementById('firmwareProgress');
  const progressBar = document.getElementById('firmwareProgressBar');
  const uploadBtn = document.getElementById('firmwareUploadBtn');
  const cancelBtn = document.getElementById('firmwareCancelBtn');

  status.textContent = 'Uploading...'; status.className = 'status';
  progress.style.display = 'block';
  uploadBtn.disabled = true;
  cancelBtn.disabled = false;

  try {
    const xhr = new XMLHttpRequest();
    const credentials = btoa('admin:' + (document.getElementById('brightness').disabled ? 'REDACTED_PASSWORD' : 'password'));
    xhr.setRequestHeader('Authorization', 'Basic ' + credentials);

    xhr.upload.addEventListener('progress', e => {
      if (e.lengthComputable) {
        const pct = Math.round((e.loaded / e.total) * 100);
        progressBar.style.width = pct + '%';
        progressBar.textContent = pct + '%';
      }
    });

    xhr.addEventListener('load', () => {
      if (!firmwareUploadActive) return;
      if (xhr.status === 200) {
        status.textContent = 'Upload complete - device restarting...'; status.className = 'status ok';
        setTimeout(() => { status.textContent = 'Device should be back online in ~10 seconds.'; }, 1000);
      } else {
        status.textContent = 'Upload failed (status ' + xhr.status + '), device should still be working.'; status.className = 'status err';
      }
      progress.style.display = 'none';
      uploadBtn.disabled = false;
      cancelBtn.disabled = true;
      firmwareUploadActive = false;
    });

    xhr.addEventListener('error', () => {
      if (!firmwareUploadActive) return;
      status.textContent = 'Upload failed - connection error'; status.className = 'status err';
      progress.style.display = 'none';
      uploadBtn.disabled = false;
      cancelBtn.disabled = true;
      firmwareUploadActive = false;
    });

    xhr.open('POST', '/api/ota');
    xhr.send(file);
  } catch (err) {
    status.textContent = 'Error: ' + err; status.className = 'status err';
    progress.style.display = 'none';
    uploadBtn.disabled = false;
    cancelBtn.disabled = true;
    firmwareUploadActive = false;
  }
});

document.getElementById('firmwareCancelBtn').addEventListener('click', () => {
  firmwareUploadActive = false;
  document.getElementById('firmwareStatus').textContent = 'Cancelled.';
  document.getElementById('firmwareStatus').className = 'status';
  document.getElementById('firmwareProgress').style.display = 'none';
  document.getElementById('firmwareUploadBtn').disabled = false;
  document.getElementById('firmwareCancelBtn').disabled = true;
});
</script>
</body></html>
)HTML";
}  // namespace

ConfigServer::ConfigServer(AppConfig& appConfig, SettingsStore& store)
    : server_(80), appConfig_(appConfig), store_(store) {}

void ConfigServer::begin() {
    server_.on("/", HTTP_GET, [this]() { handleGetRoot(); });
    server_.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
    server_.on("/api/config", HTTP_PUT, [this]() { handlePutConfig(); });
    server_.on("/api/wifi", HTTP_POST, [this]() { handlePostWifi(); });
    server_.on("/api/wifi/scan", HTTP_GET, [this]() { handleGetWifiScan(); });
    server_.on("/api/shutdown", HTTP_POST, [this]() { handlePostShutdown(); });
    server_.on("/api/wifi/forget", HTTP_POST, [this]() {
        if (!checkAuth()) return;
        server_.send(200, "application/json", "{\"status\":\"forgetting\"}");
        delay(200); // let the response flush before the network drops
        wm.resetSettings();
        delay(200);
        ESP.restart();
    });
    server_.on("/api/assets", HTTP_POST,
        [this]() { handlePostAssetResponse(); },
        [this]() { handlePostAssetUpload(); });
    server_.on("/api/screensaver", HTTP_POST,
        [this]() { handlePostScreensaverResponse(); },
        [this]() { handlePostScreensaverUpload(); });
    server_.on("/api/dnd", HTTP_POST,
        [this]() { handlePostDndResponse(); },
        [this]() { handlePostDndUpload(); });
    server_.on("/api/ota", HTTP_POST,
        [this]() {
            if (!checkAuth()) return;
            server_.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
            delay(500);
            ESP.restart();
        },
        [this]() { handleOtaUpload(); });
    server_.begin();
}

void ConfigServer::loop() { server_.handleClient(); }

bool ConfigServer::configChanged() {
    bool changed = configChanged_;
    configChanged_ = false;
    return changed;
}

bool ConfigServer::checkAuth() {
    if (server_.authenticate(CONFIG_AUTH_USER, CONFIG_AUTH_PASS)) return true;
    server_.requestAuthentication();
    return false;
}

void ConfigServer::handleGetRoot() {
    if (!checkAuth()) return;
    server_.send(200, "text/html", kConfigPageHtml);
}

void ConfigServer::handleGetConfig() {
    if (!checkAuth()) return;
    server_.send(200, "application/json", serializeConfig(appConfig_).c_str());
}

void ConfigServer::handlePutConfig() {
    if (!checkAuth()) return;
    std::string body = server_.arg("plain").c_str();
    AppConfig parsed;
    std::string error;
    if (!parseConfig(body, parsed, error)) {
        server_.send(400, "application/json", ("{\"error\":\"" + error + "\"}").c_str());
        return;
    }
    appConfig_ = parsed;
    store_.save(serializeConfig(appConfig_));
    configChanged_ = true;
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

void ConfigServer::handlePostWifi() {
    if (!checkAuth()) return;
    std::string body = server_.arg("plain").c_str();
    JsonDocument doc;
    if (deserializeJson(doc, body) || !doc["ssid"].is<const char*>() || strlen(doc["ssid"] | "") == 0) {
        server_.send(400, "application/json", "{\"error\":\"missing ssid\"}");
        return;
    }
    std::string ssid = doc["ssid"] | "";
    std::string password = doc["password"] | "";

    server_.send(200, "application/json", "{\"status\":\"reconnecting\"}");
    delay(200); // let the response flush before the network drops
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str()); // persistent by default: also becomes the auto-connect target on next boot
}

void ConfigServer::handlePostShutdown() {
    if (!checkAuth()) return;
    shutdownRequested_ = true; // DeskMatrix.ino's loop() picks this up on its next iteration
    server_.send(200, "application/json", "{\"status\":\"shutting down\"}");
}

void ConfigServer::handleGetWifiScan() {
    if (!checkAuth()) return;
    // Blocking (~2-4s) and briefly interrupts the STA connection — fine for
    // an on-demand user action from the config page, same trade-off as the
    // existing OTA endpoint. Doesn't drop the config API itself: handled
    // synchronously within this one request/response, not backgrounded.
    int count = WiFi.scanNetworks();

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    if (count > 0) {
        // Strongest-signal-first, de-duplicated by SSID (scanNetworks()
        // can return the same network multiple times across channels/APs).
        for (int i = 0; i < count; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.isEmpty()) continue;
            bool alreadyListed = false;
            for (JsonObject existing : networks) {
                if (existing["ssid"] == ssid) {
                    alreadyListed = true;
                    if (WiFi.RSSI(i) > existing["rssi"].as<int>()) {
                        existing["rssi"] = WiFi.RSSI(i);
                        existing["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
                    }
                    break;
                }
            }
            if (!alreadyListed) {
                JsonObject net = networks.add<JsonObject>();
                net["ssid"] = ssid;
                net["rssi"] = WiFi.RSSI(i);
                net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            }
        }
    }
    WiFi.scanDelete();

    std::string out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out.c_str());
}

void ConfigServer::handlePostAssetUpload() {
    // Uses server_.raw() (HTTPRaw), not server_.upload() (HTTPUpload): the
    // installed WebServer version (esp32 core 3.3.11) only populates
    // _currentUpload for multipart/form-data bodies. A plain (non-multipart)
    // POST body — e.g. `curl --data-binary` — is dispatched through the raw
    // path instead, and calling upload() there dereferences a null
    // unique_ptr (crash, verified on real hardware). See ConfigServer.h.
    if (!checkAuth()) return;
    HTTPRaw& raw = server_.raw();
    static File assetFile;

    if (raw.status == RAW_START) {
        if (!server_.hasArg("id")) return;
        if (!LittleFS.exists("/assets")) LittleFS.mkdir("/assets");
        std::string path = "/assets/" + std::string(server_.arg("id").c_str()) + ".bin";
        assetFile = LittleFS.open(path.c_str(), "w");
    } else if (raw.status == RAW_WRITE) {
        if (assetFile) assetFile.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
        if (assetFile) assetFile.close();
    }
}

void ConfigServer::handlePostAssetResponse() {
    if (!checkAuth()) return;
    if (!server_.hasArg("id")) {
        server_.send(400, "text/plain", "missing ?id=");
        return;
    }
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

namespace {
File g_screensaverTmpFile;
bool g_screensaverHeaderChecked = false;
bool g_screensaverHeaderValid = false;
// The HTTP client's raw POST body can arrive in chunks smaller than the
// 6-byte GIF signature (e.g. curl over a slow/segmented connection) —
// accumulate here until there's enough to check, rather than judging the
// header off whatever happened to be in the very first chunk.
uint8_t g_screensaverHeaderBuf[6];
size_t g_screensaverHeaderBufLen = 0;
}  // namespace

void ConfigServer::handlePostScreensaverUpload() {
    // See the comment in handlePostAssetUpload(): raw (non-multipart) POST
    // bodies go through server_.raw(), not server_.upload().
    if (!checkAuth()) return;
    HTTPRaw& raw = server_.raw();

    if (raw.status == RAW_START) {
        g_screensaverHeaderChecked = false;
        g_screensaverHeaderValid = false;
        g_screensaverHeaderBufLen = 0;
        g_screensaverTmpFile = LittleFS.open("/screensaver.gif.tmp", "w");
    } else if (raw.status == RAW_WRITE) {
        if (!g_screensaverHeaderChecked) {
            size_t need = sizeof(g_screensaverHeaderBuf) - g_screensaverHeaderBufLen;
            size_t take = raw.currentSize < need ? raw.currentSize : need;
            memcpy(g_screensaverHeaderBuf + g_screensaverHeaderBufLen, raw.buf, take);
            g_screensaverHeaderBufLen += take;
            if (g_screensaverHeaderBufLen == sizeof(g_screensaverHeaderBuf)) {
                g_screensaverHeaderValid = isValidGifHeader(g_screensaverHeaderBuf, g_screensaverHeaderBufLen);
                g_screensaverHeaderChecked = true;
            }
        }
        if (g_screensaverTmpFile) g_screensaverTmpFile.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
        if (g_screensaverTmpFile) g_screensaverTmpFile.close();
        if (g_screensaverHeaderValid) {
            // LittleFS refuses to remove/rename a file that's still open —
            // the currently-playing GIF holds /screensaver.gif open, so
            // this must run before the remove/rename below, not after.
            unloadScreensaverGif();
            if (LittleFS.exists("/screensaver.gif")) LittleFS.remove("/screensaver.gif");
            LittleFS.rename("/screensaver.gif.tmp", "/screensaver.gif");
            loadScreensaverGif(); // pick up the newly-uploaded GIF immediately, without a reboot
        } else {
            LittleFS.remove("/screensaver.gif.tmp"); // reject: previous screensaver (if any) stays active
        }
    }
}

void ConfigServer::handlePostScreensaverResponse() {
    if (!checkAuth()) return;
    if (!g_screensaverHeaderValid) {
        server_.send(400, "application/json", "{\"error\":\"not a valid GIF file\"}");
        return;
    }
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

namespace {
File g_dndTmpFile;
bool g_dndHeaderChecked = false;
bool g_dndHeaderValid = false;
uint8_t g_dndHeaderBuf[6];
size_t g_dndHeaderBufLen = 0;
}  // namespace

// Mirrors handlePostScreensaverUpload()/handlePostScreensaverResponse()
// exactly, targeting /dnd.gif instead — GIF-only (no JPEG support, unlike
// the reverted "custom image" attempt), so this adds no decoder RAM cost;
// ScreensaverScreen.cpp's shared GIF decoder picks up the new file lazily
// the next time DND is the active screen (see unloadDndGif()'s comment).
void ConfigServer::handlePostDndUpload() {
    if (!checkAuth()) return;
    HTTPRaw& raw = server_.raw();

    if (raw.status == RAW_START) {
        g_dndHeaderChecked = false;
        g_dndHeaderValid = false;
        g_dndHeaderBufLen = 0;
        g_dndTmpFile = LittleFS.open("/dnd.gif.tmp", "w");
    } else if (raw.status == RAW_WRITE) {
        if (!g_dndHeaderChecked) {
            size_t need = sizeof(g_dndHeaderBuf) - g_dndHeaderBufLen;
            size_t take = raw.currentSize < need ? raw.currentSize : need;
            memcpy(g_dndHeaderBuf + g_dndHeaderBufLen, raw.buf, take);
            g_dndHeaderBufLen += take;
            if (g_dndHeaderBufLen == sizeof(g_dndHeaderBuf)) {
                g_dndHeaderValid = isValidGifHeader(g_dndHeaderBuf, g_dndHeaderBufLen);
                g_dndHeaderChecked = true;
            }
        }
        if (g_dndTmpFile) g_dndTmpFile.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
        if (g_dndTmpFile) g_dndTmpFile.close();
        if (g_dndHeaderValid) {
            unloadDndGif(); // release the shared decoder's hold on /dnd.gif before overwriting it
            if (LittleFS.exists("/dnd.gif")) LittleFS.remove("/dnd.gif");
            LittleFS.rename("/dnd.gif.tmp", "/dnd.gif");
        } else {
            LittleFS.remove("/dnd.gif.tmp"); // reject: previous DND gif (if any) stays active
        }
    }
}

void ConfigServer::handlePostDndResponse() {
    if (!checkAuth()) return;
    if (!g_dndHeaderValid) {
        server_.send(400, "application/json", "{\"error\":\"not a valid GIF file\"}");
        return;
    }
    server_.send(200, "application/json", "{\"status\":\"ok\"}");
}

void ConfigServer::handleOtaUpload() {
    // See the comment in handlePostAssetUpload(): raw (non-multipart) POST
    // bodies go through server_.raw(), not server_.upload().
    if (!checkAuth()) return;
    HTTPRaw& raw = server_.raw();
    if (raw.status == RAW_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (raw.status == RAW_WRITE) {
        Update.write(raw.buf, raw.currentSize);
    } else if (raw.status == RAW_END) {
        Update.end(true);
    }
}
