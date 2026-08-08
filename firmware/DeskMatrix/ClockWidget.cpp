// firmware/DeskMatrix/ClockWidget.cpp
#include "screens/ClockWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include <cstdlib>
#include "ColorUtil.h"

static void drawDigitalClock(const RenderContext& ctx) {
    auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
    if (!display || !ctx.widget) return;

    time_t now = time(nullptr);
    struct tm timeInfo;
    localtime_r(&now, &timeInfo);

    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);

    uint16_t color = display->color565(255, 222, 89);
    uint8_t r, g, b;
    if (parseHexColor(ctx.widget->color, r, g, b)) {
        color = display->color565(r, g, b);
    }

    display->setTextSize(1);
    display->setTextColor(color);
    display->setCursor(ctx.widget->x, ctx.widget->y);
    display->print(buf);
}

void registerClockWidget(WidgetRegistry& registry) {
    registry.registerType("clock", drawDigitalClock);
}
