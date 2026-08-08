// firmware/DeskMatrix/WeatherWidget.cpp
#include "screens/WeatherWidget.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

void registerWeatherWidget(WidgetRegistry& registry, WeatherService& weatherService) {
    registry.registerType("weather", [&weatherService](const RenderContext& ctx) {
        auto* display = static_cast<MatrixPanel_I2S_DMA*>(ctx.displayHandle);
        if (!display || !ctx.widget) return;

        WeatherReading r = weatherService.latest();
        display->setTextSize(1);
        display->setCursor(ctx.widget->x, ctx.widget->y);
        display->setTextColor(display->color565(76, 154, 255));

        if (!r.valid) {
            display->print("--");
            return;
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", r.temperatureC);
        display->print(buf);
    });
}
