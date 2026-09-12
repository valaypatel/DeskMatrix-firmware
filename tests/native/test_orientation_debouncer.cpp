// tests/native/test_orientation_debouncer.cpp
#include "test_framework.h"
#include "OrientationDebouncer.h"

int main() {
    OrientationDebouncer d(5);

    // Normal readings stay NORMAL
    for (int i = 0; i < 10; i++) CHECK(d.update(PanelOrientation::NORMAL) == PanelOrientation::NORMAL);

    // A single spurious reading doesn't trip it
    CHECK(d.update(PanelOrientation::DND) == PanelOrientation::NORMAL);
    CHECK(d.update(PanelOrientation::NORMAL) == PanelOrientation::NORMAL);

    // 5 consecutive readings -> DND
    PanelOrientation last = PanelOrientation::NORMAL;
    for (int i = 0; i < 5; i++) last = d.update(PanelOrientation::DND);
    CHECK(last == PanelOrientation::DND);

    // Returning to normal for 5 readings -> NORMAL
    for (int i = 0; i < 5; i++) last = d.update(PanelOrientation::NORMAL);
    CHECK(last == PanelOrientation::NORMAL);

    // BRB works the same way
    for (int i = 0; i < 5; i++) last = d.update(PanelOrientation::BRB);
    CHECK(last == PanelOrientation::BRB);

    TEST_SUMMARY();
}
