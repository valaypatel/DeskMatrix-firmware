#pragma once
#include <string>

// Maps Open-Meteo's WMO weather_code (https://open-meteo.com/en/docs, "WMO
// Weather interpretation codes") plus is_day to one of this project's four
// icon ids. Snow and thunderstorm codes fall back to "rainy" since this
// MVP's icon set doesn't include dedicated snow/storm icons — precipitation
// is the closest visual match.
inline std::string weatherIconId(int wmoCode, bool isDay) {
    if (wmoCode == 0 || wmoCode == 1) {
        return isDay ? "weather_sunny" : "weather_clear_night";
    }
    if (wmoCode == 2 || wmoCode == 3 || wmoCode == 45 || wmoCode == 48) {
        return "weather_cloudy";
    }
    bool isPrecipitation =
        (wmoCode >= 51 && wmoCode <= 67) ||   // drizzle, rain, freezing rain
        (wmoCode >= 71 && wmoCode <= 77) ||   // snow
        (wmoCode >= 80 && wmoCode <= 86) ||   // showers (rain or snow)
        (wmoCode == 95 || wmoCode == 96 || wmoCode == 99); // thunderstorm
    if (isPrecipitation) {
        return "weather_rainy";
    }
    return "weather_cloudy"; // safe fallback for anything unrecognized
}
