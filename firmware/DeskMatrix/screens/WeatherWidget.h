// firmware/DeskMatrix/screens/WeatherWidget.h
#pragma once
#include "../WidgetRegistry.h"
#include "../services/WeatherService.h"

void registerWeatherWidget(WidgetRegistry& registry, WeatherService& weatherService);
