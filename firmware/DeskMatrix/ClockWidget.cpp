// firmware/DeskMatrix/ClockWidget.cpp
#include "screens/ClockWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include <cstdlib>
#include "ColorUtil.h"
#include "IconRenderer.h"

static const char* kDayNames[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void registerClockWidget(WidgetRegistry& registry, RemoteClockService& remoteClockService) {
    registry.registerType("clock", [&remoteClockService](const RenderContext& ctx) {
        auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
        if (!display || !ctx.widget) return;

        struct tm timeInfo;
        bool isRemote = !ctx.widget->location.empty();
        bool showDay = false;

        if (isRemote) {
            time_t nowUtc = time(nullptr);
            if (remoteClockService.hasOffset()) {
                timeInfo = computeRemoteTm(nowUtc, remoteClockService.offsetSeconds());
            } else {
                // No offset fetched yet: show the device's own local time
                // rather than a blank/garbage clock until the first fetch lands.
                localtime_r(&nowUtc, &timeInfo);
            }
        } else {
            time_t now = time(nullptr);
            localtime_r(&now, &timeInfo);
            showDay = (ctx.widget->style == "digital_with_day");
        }

        int hour12 = timeInfo.tm_hour % 12;
        if (hour12 == 0) hour12 = 12;
        const char* ampm = (timeInfo.tm_hour < 12) ? "AM" : "PM";

        char buf[10];
        snprintf(buf, sizeof(buf), "%d:%02d%s", hour12, timeInfo.tm_min, ampm);

        uint16_t color = display->color565(255, 222, 89);
        uint8_t r, g, b;
        if (parseHexColor(ctx.widget->color, r, g, b)) {
            color = display->color565(r, g, b);
        }

        // Flag icon, if configured, sits just left of the time text. Exact
        // offset/size (12x8 here) is a starting point — confirm/adjust by
        // looking at the real panel once a real flag asset is uploaded
        // (Task 10), since legibility at this scale can only really be
        // judged on the actual hardware.
        int textX = ctx.widget->x;
        if (!ctx.widget->icon.empty()) {
            drawIcon(display, ctx.widget->icon, ctx.widget->x, ctx.widget->y, 12, 8);
            textX = ctx.widget->x + 14;
        }

        display->setTextSize(1);
        display->setTextColor(color);
        display->setCursor(textX, ctx.widget->y);
        display->print(buf);

        if (showDay) {
            display->setCursor(ctx.widget->x, ctx.widget->y + 8);
            display->print(kDayNames[timeInfo.tm_wday]);
        }
    });
}
