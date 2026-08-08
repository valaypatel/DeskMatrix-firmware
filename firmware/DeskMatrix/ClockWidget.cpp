// firmware/DeskMatrix/ClockWidget.cpp
#include "screens/ClockWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include <cstdlib>

static void drawDigitalClock(const RenderContext& ctx) {
    auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
    if (!display || !ctx.widget) return;

    time_t now = time(nullptr);
    struct tm timeInfo;
    localtime_r(&now, &timeInfo);

    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);

    uint16_t color = display->color565(255, 222, 89);
    if (ctx.widget->color.size() == 7 && ctx.widget->color[0] == '#') {
        long rgb = strtol(ctx.widget->color.c_str() + 1, nullptr, 16);
        color = display->color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    display->setTextSize(1);
    display->setTextColor(color);
    display->setCursor(ctx.widget->x, ctx.widget->y);
    display->print(buf);
}

void registerClockWidget(WidgetRegistry& registry) {
    registry.registerType("clock", drawDigitalClock);
}
