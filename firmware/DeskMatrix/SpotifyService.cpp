// firmware/DeskMatrix/SpotifyService.cpp
#include "services/SpotifyService.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>

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
    static bool lastRefreshOk = false;
    static unsigned long nextRetryAllowedMs = 0;
    static int consecutiveFailures = 0;

    if (!spotify || lastClientId != clientId_ || lastRefreshToken != refreshToken_) {
        // Credentials just changed (including the first call ever) — retry
        // immediately and forget any backoff from a previous, now-irrelevant
        // set of credentials.
        nextRetryAllowedMs = 0;
        consecutiveFailures = 0;
    }
    if (!lastRefreshOk && millis() < nextRetryAllowedMs) {
        return; // backing off after repeated failures; see below
    }

    // Re-create the SpotifyArduino instance whenever credentials actually
    // change (including the first call) — the library takes them by pointer
    // at construction, so it can't be reconfigured in place. Also retry
    // whenever the last refresh attempt failed (e.g. the very first poll
    // can hit this before NTP has finished syncing, which breaks TLS
    // certificate-date validation) — otherwise a single transient failure
    // permanently poisons the session with an empty bearer token, since
    // nothing else here would ever trigger a retry.
    if (!spotify || lastClientId != clientId_ || lastRefreshToken != refreshToken_ || !lastRefreshOk) {
        delete spotify;
        // The vendored SpotifyArduinoCert.h root cert no longer validates
        // against Spotify's current TLS chain (confirmed on real hardware:
        // every connect() with setCACert() failed for accounts.spotify.com
        // and api.spotify.com; switching to setInsecure() fixed both
        // immediately). Traffic stays encrypted; only server identity
        // verification is skipped. Accepted trade-off — see project notes.
        client.setInsecure();
        // WiFiClientSecure's defaults (30s connect, 120s TLS handshake) let a
        // single failing/degraded connection to Spotify's servers block the
        // entire main loop() -- freezing the display and the config web
        // server -- for up to 2.5 minutes per attempt (confirmed on real
        // hardware: enabling Spotify with a bad network path froze the
        // matrix and made the config page unreachable). A first pass bounded
        // both to ~4s/5s, but that was too tight for this network's real
        // round-trip to Spotify's servers: the one connection that succeeded
        // during the original incident took up to ~25s. 10s/10s (~20s worst
        // case) still bounds a single failed attempt far below the original
        // 2.5-minute freeze, while giving a genuinely slow-but-working
        // connection a real chance -- and with the exponential backoff below,
        // a 20s stall only happens once every 10s-2min, not every poll.
        client.setConnectionTimeout(10000);
        client.setHandshakeTimeout(10); // seconds, per NetworkClientSecure::setHandshakeTimeout
        spotify = new SpotifyArduino(client, clientId_.c_str(), clientSecret_.c_str(), refreshToken_.c_str());
        lastRefreshOk = spotify->refreshAccessToken();
        if (!lastRefreshOk) {
            // SpotifyArduino's connect() failure path returns without
            // calling client.stop() (confirmed in the vendored library
            // source), leaking a partial socket/TLS context on `client`
            // every failed attempt. Left unchecked at the previous
            // every-pollSec retry rate, repeated failures (e.g. a bad
            // refresh token) exhaust the device's small pool of TLS
            // sessions/heap fast enough to degrade unrelated network
            // activity -- confirmed on real hardware: the config web server
            // became unreliable, and in one run the whole device hung,
            // purely from Spotify's connect() retries failing repeatedly.
            // Cleaning up here plus backing off (below) keeps failed
            // attempts rare and each one self-contained.
            client.stop();
            consecutiveFailures++;
            // Exponential backoff (pollSec, then 2x, 4x, ... capped at 2
            // minutes) instead of retrying every single poll.
            unsigned long backoffMs = (unsigned long)pollSec_ * 1000UL * (1UL << (consecutiveFailures < 5 ? consecutiveFailures : 5));
            if (backoffMs > 120000UL) backoffMs = 120000UL;
            nextRetryAllowedMs = millis() + backoffMs;
            Serial.printf("[spotify] failed to refresh access token, backing off %lums\n", backoffMs);
        } else {
            consecutiveFailures = 0;
        }
        lastClientId = clientId_;
        lastRefreshToken = refreshToken_;
    }
    if (!lastRefreshOk) return; // nothing more to do until the backoff above expires

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
