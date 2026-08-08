#include "test_framework.h"
#include "ScreenStateMachine.h"

int main() {
    ScreenStateMachine sm;
    CHECK(sm.mode() == ScreenMode::WIFI_SETUP);

    sm.wifiConfigured();
    CHECK(sm.mode() == ScreenMode::HOME);

    sm.enterTakeover();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);

    sm.exitTakeover();
    CHECK(sm.mode() == ScreenMode::HOME);

    // Tilt overrides HOME, and returns to HOME on center
    sm.tiltLeft();
    CHECK(sm.mode() == ScreenMode::DND);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::HOME);

    // Tilt overrides INTERRUPT_TAKEOVER, and restores it (not HOME) on center
    sm.enterTakeover();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);
    sm.tiltRight();
    CHECK(sm.mode() == ScreenMode::BRB);
    sm.tiltCenter();
    CHECK(sm.mode() == ScreenMode::INTERRUPT_TAKEOVER);

    // Guard: enterTakeover() has no effect outside HOME
    ScreenStateMachine sm2;
    sm2.enterTakeover();
    CHECK(sm2.mode() == ScreenMode::WIFI_SETUP);

    TEST_SUMMARY();
}
