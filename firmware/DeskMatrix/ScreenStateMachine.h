#pragma once

enum class ScreenMode {
    WIFI_SETUP,
    HOME,
    INTERRUPT_TAKEOVER,
    DND,
    BRB
};

class ScreenStateMachine {
public:
    ScreenStateMachine() : mode_(ScreenMode::WIFI_SETUP), preTiltMode_(ScreenMode::HOME) {}

    ScreenMode mode() const { return mode_; }

    void wifiConfigured() {
        if (mode_ == ScreenMode::WIFI_SETUP) mode_ = ScreenMode::HOME;
    }

    void enterTakeover() {
        if (mode_ == ScreenMode::HOME) mode_ = ScreenMode::INTERRUPT_TAKEOVER;
    }

    void exitTakeover() {
        if (mode_ == ScreenMode::INTERRUPT_TAKEOVER) mode_ = ScreenMode::HOME;
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
