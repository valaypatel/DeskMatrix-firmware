#include "test_framework.h"
#include "ScreenStateMachine.h"

int main() {
    ScreenStateMachine sm;
    CHECK(sm.mode() == ScreenMode::WIFI_SETUP);

    sm.wifiConfigured();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Spotify starting/stopping toggles SCREENSAVER <-> SPOTIFY_PLAYING
    sm.spotifyStarted();
    CHECK(sm.mode() == ScreenMode::SPOTIFY_PLAYING);
    sm.spotifyStopped();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Guard: spotifyStarted() has no effect outside SCREENSAVER
    sm.enterTakeover();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);
    sm.spotifyStarted();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER); // unchanged
    sm.exitTakeover();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Guard: spotifyStopped() has no effect outside SPOTIFY_PLAYING
    sm.spotifyStopped();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER); // unchanged, already wasn't playing

    // Tilt overrides SCREENSAVER, and returns to SCREENSAVER on center
    sm.tiltLeft();
    CHECK(sm.mode() == ScreenMode::DND);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::SCREENSAVER);

    // Tilt overrides SPOTIFY_PLAYING, and restores it (not SCREENSAVER) on center
    sm.spotifyStarted();
    CHECK(sm.mode() == ScreenMode::SPOTIFY_PLAYING);
    sm.tiltRight();
    CHECK(sm.mode() == ScreenMode::BRB);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::SPOTIFY_PLAYING);

    // Guard: enterTakeover() has no effect outside SCREENSAVER
    ScreenStateMachine sm2;
    sm2.enterTakeover();
    CHECK(sm2.mode() == ScreenMode::WIFI_SETUP);

    TEST_SUMMARY();
}
