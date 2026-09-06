// firmware/DeskMatrix/SpotifyService.cpp
#include "services/SpotifyService.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

namespace {
// SpotifyArduino's getCurrentlyPlaying callback is a plain function pointer
// with no captured state, so poll()'s result is staged here and copied into
// SpotifyService::latest_ immediately after the (synchronous) call returns.
bool g_pendingIsPlaying = false;
std::string g_pendingArtUrl;

void onCurrentlyPlaying(CurrentlyPlaying currentlyPlaying) {
    g_pendingIsPlaying = currentlyPlaying.isPlaying;
    g_pendingArtUrl.clear();
    // Spotify returns album art in 3 sizes (large/medium/small); index 2 is
    // the smallest, matching this panel's 64x64 resolution — same choice
    // used by https://github.com/dylduhamel/spotify_esp32_led_matrix, which
    // this integration is modeled on.
    if (currentlyPlaying.numImages > 2 && currentlyPlaying.albumImages[2].url) {
        g_pendingArtUrl = currentlyPlaying.albumImages[2].url;
    }
}
}  // namespace

SpotifyService::SpotifyService(const std::string& clientId, const std::string& clientSecret,
                                const std::string& refreshToken, int pollSec)
    : clientId_(clientId), clientSecret_(clientSecret), refreshToken_(refreshToken), pollSec_(pollSec) {}

void SpotifyService::configure(const std::string& clientId, const std::string& clientSecret,
                                const std::string& refreshToken, int pollSec) {
    clientId_ = clientId;
    clientSecret_ = clientSecret;
    refreshToken_ = refreshToken;
    pollSec_ = pollSec;
    lastPollMs_ = 0; // force an immediate poll on the next loop()
}

void SpotifyService::loop() {
    unsigned long nowMs = millis();
    if (lastPollMs_ != 0 && (nowMs - lastPollMs_) < (unsigned long)pollSec_ * 1000UL) {
        return;
    }
    poll();
    lastPollMs_ = nowMs;
}

void SpotifyService::poll() {
    if (clientId_.empty() || clientSecret_.empty() || refreshToken_.empty()) {
        return; // not configured yet, per the design spec's error handling
    }
    if (WiFi.status() != WL_CONNECTED) return;

    static WiFiClientSecure client;
    static SpotifyArduino* spotify = nullptr;
    static std::string lastClientId, lastRefreshToken;

    // Re-create the SpotifyArduino instance whenever credentials actually
    // change (including the first call) — the library takes them by pointer
    // at construction, so it can't be reconfigured in place.
    if (!spotify || lastClientId != clientId_ || lastRefreshToken != refreshToken_) {
        delete spotify;
        client.setCACert(spotify_server_cert);
        spotify = new SpotifyArduino(client, clientId_.c_str(), clientSecret_.c_str(), refreshToken_.c_str());
        if (!spotify->refreshAccessToken()) {
            Serial.println("[spotify] failed to refresh access token");
        }
        lastClientId = clientId_;
        lastRefreshToken = refreshToken_;
    }

    g_pendingIsPlaying = false;
    g_pendingArtUrl.clear();
    int status = spotify->getCurrentlyPlaying(onCurrentlyPlaying, "");
    if (status == 200) {
        latest_.isPlaying = g_pendingIsPlaying;
        latest_.albumArtUrl = g_pendingArtUrl;
    } else if (status == 204) {
        latest_.isPlaying = false; // Spotify's contract for "nothing playing"
    } else {
        Serial.printf("[spotify] getCurrentlyPlaying failed, status=%d\n", status);
        // keep last-known-good status, per the design spec's error handling
    }
}
