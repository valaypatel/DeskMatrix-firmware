#pragma once

enum class ScreenMode {
    WIFI_SETUP,
    SCREENSAVER,
    SPOTIFY_PLAYING,
    INTERRUPT_TAKEOVER,
    DND,
    BRB
};

class ScreenStateMachine {
public:
    ScreenStateMachine() : mode_(ScreenMode::WIFI_SETUP), preTiltMode_(ScreenMode::SCREENSAVER) {}

    ScreenMode mode() const { return mode_; }

    void wifiConfigured() {
        if (mode_ == ScreenMode::WIFI_SETUP) mode_ = ScreenMode::SCREENSAVER;
    }

    // Entered when Spotify polling reports active playback; only takes effect
    // from SCREENSAVER so DND/BRB/INTERRUPT_TAKEOVER keep their priority.
    void spotifyStarted() {
        if (mode_ == ScreenMode::SCREENSAVER) mode_ = ScreenMode::SPOTIFY_PLAYING;
    }

    // Entered when Spotify polling reports playback stopped/paused.
    void spotifyStopped() {
        if (mode_ == ScreenMode::SPOTIFY_PLAYING) mode_ = ScreenMode::SCREENSAVER;
    }

    void enterTakeover() {
        if (mode_ == ScreenMode::SCREENSAVER) mode_ = ScreenMode::INTERRUPT_TAKEOVER;
    }

    void exitTakeover() {
        if (mode_ == ScreenMode::INTERRUPT_TAKEOVER) mode_ = ScreenMode::SCREENSAVER;
    }

    void tiltLeft() {
        if (mode_ != ScreenMode::DND && mode_ != ScreenMode::BRB) preTiltMode_ = mode_;
        mode_ = ScreenMode::DND;
    }

    void tiltRight() {
        if (mode_ != ScreenMode::DND && mode_ != ScreenMode::BRB) preTiltMode_ = mode_;
        mode_ = ScreenMode::BRB;
    }

    void tiltCenter() {
        if (mode_ == ScreenMode::DND || mode_ == ScreenMode::BRB) mode_ = preTiltMode_;
    }

private:
    ScreenMode mode_;
    ScreenMode preTiltMode_;
};
