// firmware/DeskMatrix/services/WeatherService.h
#pragma once

struct WeatherReading {
    float temperatureC = 0;
    int weatherCode = 0;
    bool valid = false;
};

class WeatherService {
public:
    WeatherService(float latitude, float longitude, int pollSec);
    void loop(); // call every loop() iteration; internally rate-limits to pollSec
    WeatherReading latest() const { return latest_; }

private:
    void fetch();
    float lat_, lon_;
    int pollSec_;
    unsigned long lastFetchMs_ = 0;
    WeatherReading latest_;
};
