// firmware/DeskMatrix/WeatherWidget.cpp
#include "screens/WeatherWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ColorUtil.h"
#include "IconRenderer.h"
#include "services/WeatherIconMap.h"

void registerWeatherWidget(WidgetRegistry& registry, WeatherService& weatherService) {
    registry.registerType("weather", [&weatherService](const RenderContext& ctx) {
        auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
        if (!display || !ctx.widget) return;

        WeatherReading r = weatherService.latest();
        display->setTextSize(1);
        display->setCursor(ctx.widget->x, ctx.widget->y);

        uint16_t color = display->color565(76, 154, 255);
        uint8_t red, green, blue;
        if (parseHexColor(ctx.widget->color, red, green, blue)) {
            color = display->color565(red, green, blue);
        }
        display->setTextColor(color);

        if (!r.valid) {
            display->print("--");
            return;
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", r.temperatureC);
        display->print(buf);

        std::string iconId = weatherIconId(r.weatherCode, r.isDay);
        // Icon fills most of the widget's cell above the temperature text.
        // Exact size/position is a starting point — confirm on the real
        // panel once a real weather icon asset is uploaded (Task 10).
        drawIcon(display, iconId, ctx.widget->x, ctx.widget->y, ctx.widget->w, ctx.widget->w > 12 ? ctx.widget->w - 8 : ctx.widget->w);
    });
}
