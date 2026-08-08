// firmware/DeskMatrix/ClockWidget.cpp
#include "screens/ClockWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include <cstdlib>
#include "ColorUtil.h"
#include "IconRenderer.h"
#include <Fonts/TomThumb.h>

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

        // Flag icon sits flush left of the time text, 12x9 (exact 4:3,
        // matching the source 640x480 artwork) — bumped up from 9x7 for
        // more real pixels to render the chakra/cross detail in, since at
        // very small sizes nearest-neighbor downsampling of thin lines
        // (chakra spokes, flag cross) loses/aliases detail regardless of
        // aspect ratio being technically correct.
        int textX = ctx.widget->x;
        if (!ctx.widget->icon.empty()) {
            drawIcon(display, ctx.widget->icon, ctx.widget->x, ctx.widget->y, 12, 9);
            textX = ctx.widget->x + 13;
        }

        // TomThumb is a compact 3x5 pixel font (bundled with Adafruit GFX) —
        // the default 5x7 font is too wide to fit "10:42 AM" plus a flag
        // icon in a 32px-wide cell. Custom-font cursor Y is the glyph's
        // baseline (bottom row), not its top-left corner: for a 5px-tall
        // glyph centered in a 9px row (2px margin top/bottom, rows 2-6),
        // baseline = widget_y + 7.
        display->setFont(&TomThumb);
        display->setTextSize(1);
        display->setTextColor(color);
        display->setCursor(textX, ctx.widget->y + 7);
        display->print(buf);

        if (showDay) {
            // Day is the third row of the local/remote clock block, laid
            // out relative to the local widget's y (=2, the 2px top
            // padding): row1 (local time) at y+0, row2 (remote time,
            // separate widget) at absolute y=12 (2 + 9 row1 + 1 gap), row3
            // (day, 5px tall — exactly the glyph height, no centering
            // needed) at absolute y=22 (12 + 9 row2 + 1 gap) -> relative to
            // this widget's y=2, that's +20, plus the +5 baseline offset
            // (5px glyph fills the 5px row exactly).
            display->setCursor(ctx.widget->x, ctx.widget->y + 25);
            display->print(kDayNames[timeInfo.tm_wday]);
        }
        display->setFont(nullptr); // revert to default font for other widgets
    });
}
