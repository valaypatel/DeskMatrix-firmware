#include "test_framework.h"
#include "services/WeatherIconMap.h"

int main() {
    // Clear sky (0) and mainly clear (1): day -> sunny, night -> clear_night
    CHECK_EQ(weatherIconId(0, true), std::string("weather_sunny"));
    CHECK_EQ(weatherIconId(0, false), std::string("weather_clear_night"));
    CHECK_EQ(weatherIconId(1, true), std::string("weather_sunny"));
    CHECK_EQ(weatherIconId(1, false), std::string("weather_clear_night"));

    // Partly cloudy / overcast / fog: always cloudy, day or night
    CHECK_EQ(weatherIconId(2, true), std::string("weather_cloudy"));
    CHECK_EQ(weatherIconId(3, false), std::string("weather_cloudy"));
    CHECK_EQ(weatherIconId(45, true), std::string("weather_cloudy"));
    CHECK_EQ(weatherIconId(48, false), std::string("weather_cloudy"));

    // Drizzle, rain, rain showers, freezing rain: rainy
    CHECK_EQ(weatherIconId(51, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(61, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(65, false), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(80, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(56, true), std::string("weather_rainy"));

    // Snow and thunderstorms: no dedicated icon in this MVP's 4-icon set,
    // fall back to rainy (closest visual match — precipitation)
    CHECK_EQ(weatherIconId(71, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(85, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(95, true), std::string("weather_rainy"));
    CHECK_EQ(weatherIconId(99, false), std::string("weather_rainy"));

    // Unknown/unexpected code: safe fallback
    CHECK_EQ(weatherIconId(9999, true), std::string("weather_cloudy"));

    TEST_SUMMARY();
}
