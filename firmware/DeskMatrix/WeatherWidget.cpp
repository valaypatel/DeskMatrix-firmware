// firmware/DeskMatrix/WeatherWidget.cpp
#include "screens/WeatherWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/TomThumb.h>
#include "ColorUtil.h"
#include "IconRenderer.h"
#include "services/WeatherIconMap.h"

void registerWeatherWidget(WidgetRegistry& registry, WeatherService& weatherService) {
    registry.registerType("weather", [&weatherService](const RenderContext& ctx) {
        auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
        if (!display || !ctx.widget) return;

        WeatherReading r = weatherService.latest();

        uint16_t color = display->color565(76, 154, 255);
        uint8_t red, green, blue;
        if (parseHexColor(ctx.widget->color, red, green, blue)) {
            color = display->color565(red, green, blue);
        }

        display->setFont(&TomThumb);
        display->setTextSize(1);
        display->setTextColor(color);

        // Icon is right-aligned in the cell; temperature text sits below it,
        // centered under the icon (not the whole cell) so the two line up.
        int iconSize = ctx.widget->w > 8 ? ctx.widget->w - 8 : ctx.widget->w;
        int iconX = ctx.widget->x + ctx.widget->w - iconSize;

        if (!r.valid) {
            int16_t bx, by; uint16_t bw, bh;
            display->getTextBounds("--", 0, 0, &bx, &by, &bw, &bh);
            int textX = iconX + (iconSize - (int)bw) / 2 - bx;
            display->setCursor(textX, ctx.widget->y + 6);
            display->print("--");
            display->setFont(nullptr);
            return;
        }

        // Icon occupies the top of the cell, temperature text sits in the
        // remaining strip below it so the two never overlap (previously the
        // icon was drawn over the same top-left origin as the text and
        // fully hid it).
        std::string iconId = weatherIconId(r.weatherCode, r.isDay);
        drawIcon(display, iconId, iconX, ctx.widget->y, iconSize, iconSize);

        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", r.temperatureC);
        int16_t bx, by; uint16_t bw, bh;
        display->getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
        int textX = iconX + (iconSize - (int)bw) / 2 - bx; // center under the icon
        display->setCursor(textX, ctx.widget->y + iconSize + 6);
        display->print(buf);
        display->setFont(nullptr); // revert to default font for other widgets
    });
}
