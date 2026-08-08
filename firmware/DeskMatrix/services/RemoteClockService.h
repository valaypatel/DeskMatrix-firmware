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
