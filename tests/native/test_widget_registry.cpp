// tests/native/test_widget_registry.cpp
#include "test_framework.h"
#include "WidgetRegistry.h"

int main() {
    WidgetRegistry registry;
    CHECK(!registry.has("clock"));

    bool called = false;
    registry.registerType("clock", [&called](const RenderContext&) { called = true; });
    CHECK(registry.has("clock"));

    WidgetConfig w;
    w.id = "w1"; w.type = "clock";
    RenderContext ctx{nullptr, &w};
    CHECK(registry.draw("clock", ctx));
    CHECK(called);

    CHECK(!registry.draw("nonexistent_type", ctx)); // returns false, doesn't crash

    TEST_SUMMARY();
}
