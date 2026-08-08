#pragma once
#include <ctime>

// Computes the calendar/time breakdown for a UTC epoch shifted by a fixed
// offset. `time(nullptr)` on the device always returns UTC regardless of
// what configTime()'s offset is set to (that only affects localtime_r()),
// so this lets a "remote" clock show a different timezone than whatever
// the device's own configTime() is currently set to, without needing a
// full timezone database — the caller supplies the offset (from Open-Meteo's
// timezone=auto, fetched by RemoteClockService in a later task).
inline struct tm computeRemoteTm(time_t utcEpoch, long offsetSec) {
    time_t shifted = utcEpoch + offsetSec;
    struct tm result;
    gmtime_r(&shifted, &result); // gmtime_r applies NO further offset — shifted is now the "local" instant
    return result;
}

// Periodically fetches the UTC offset for a fixed lat/lon from Open-Meteo
// (timezone=auto), for use with computeRemoteTm() to render a "remote" clock
// widget. Rate-limits itself to pollIntervalMs between fetches.
class RemoteClockService {
public:
    RemoteClockService(float latitude, float longitude, unsigned long pollIntervalMs);
    void loop(); // call every loop() iteration; internally rate-limits to pollIntervalMs

    bool hasOffset() const { return haveOffset_; }
    long offsetSeconds() const { return offsetSec_; }

private:
    void fetch();
    float lat_, lon_;
    unsigned long pollIntervalMs_;
    unsigned long lastFetchMs_ = 0;
    long offsetSec_ = 0;
    bool haveOffset_ = false;
};
