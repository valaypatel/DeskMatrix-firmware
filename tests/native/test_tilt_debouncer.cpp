// tests/native/test_tilt_debouncer.cpp
#include "test_framework.h"
#include "TiltDebouncer.h"

int main() {
    TiltDebouncer d(25.0f, 5);

    // Level readings stay CENTER
    for (int i = 0; i < 10; i++) CHECK(d.update(0.0f) == TiltDirection::CENTER);

    // A single spurious spike doesn't trip it
    CHECK(d.update(-30.0f) == TiltDirection::CENTER);
    CHECK(d.update(0.0f) == TiltDirection::CENTER);

    // 5 consecutive readings past threshold -> LEFT
    TiltDirection last = TiltDirection::CENTER;
    for (int i = 0; i < 5; i++) last = d.update(-30.0f);
    CHECK(last == TiltDirection::LEFT);

    // Returning to level for 5 readings -> CENTER
    for (int i = 0; i < 5; i++) last = d.update(0.0f);
    CHECK(last == TiltDirection::CENTER);

    // Right tilt works the same way
    for (int i = 0; i < 5; i++) last = d.update(30.0f);
    CHECK(last == TiltDirection::RIGHT);

    TEST_SUMMARY();
}
