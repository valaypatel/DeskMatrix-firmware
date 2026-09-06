// firmware/DeskMatrix/services/SpotifyService.h
#pragma once
#include <string>

struct SpotifyStatus {
    bool isPlaying = false;
    std::string albumArtUrl; // empty if nothing playing / no valid reading yet
};

class SpotifyService {
public:
    SpotifyService(const std::string& clientId, const std::string& clientSecret,
                   const std::string& refreshToken, int pollSec);
    void loop(); // call every loop() iteration; internally rate-limits to pollSec
    SpotifyStatus latest() const { return latest_; }

    // Updates credentials/poll interval (e.g. after a config push) and resets
    // the poll timer so loop() polls immediately on its next call.
    void configure(const std::string& clientId, const std::string& clientSecret,
                   const std::string& refreshToken, int pollSec);

private:
    void poll();
    std::string clientId_, clientSecret_, refreshToken_;
    int pollSec_;
    unsigned long lastPollMs_ = 0;
    SpotifyStatus latest_;
};

// Pure helper: true when `newUrl` is non-empty and different from
// `previousUrl`, meaning a caller should re-fetch/re-decode album art rather
// than reuse what's already drawn.
// Defined inline (like IconRenderer.h's sampleIconPixel()) so native tests
// can exercise it without linking SpotifyService.cpp, which pulls in
// Arduino-only symbols (millis(), WiFi, SpotifyArduino) that don't exist on
// the host.
inline bool albumArtChanged(const std::string& previousUrl, const std::string& newUrl) {
    return !newUrl.empty() && newUrl != previousUrl;
}
