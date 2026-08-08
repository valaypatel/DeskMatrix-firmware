#include "test_framework.h"
#include "services/RemoteClockService.h"

int main() {
    // 2026-01-15 12:00:00 UTC = epoch 1768478400 (a fixed, known reference instant)
    time_t utcNoon = 1768478400;

    // UTC+0: identical to input
    struct tm t0 = computeRemoteTm(utcNoon, 0);
    CHECK_EQ(t0.tm_hour, 12);
    CHECK_EQ(t0.tm_min, 0);

    // UTC+5:30 (India, as a sanity check against a known real offset)
    struct tm tIst = computeRemoteTm(utcNoon, 5 * 3600 + 30 * 60);
    CHECK_EQ(tIst.tm_hour, 17);
    CHECK_EQ(tIst.tm_min, 30);

    // UTC+0 (London GMT, winter/no DST) — same as t0 in this scenario
    struct tm tLondonWinter = computeRemoteTm(utcNoon, 0);
    CHECK_EQ(tLondonWinter.tm_hour, 12);

    // UTC+1 (London BST, summer DST) — offset is just a number, DST handling
    // lives entirely in what Open-Meteo's timezone=auto reports, not here
    struct tm tLondonSummer = computeRemoteTm(utcNoon, 3600);
    CHECK_EQ(tLondonSummer.tm_hour, 13);

    // Negative offset (US Eastern, UTC-5)
    struct tm tUsEast = computeRemoteTm(utcNoon, -5 * 3600);
    CHECK_EQ(tUsEast.tm_hour, 7);

    // Crossing midnight forward (large positive offset pushes into next day)
    time_t utcLateEvening = utcNoon + 13 * 3600; // 2026-01-16 01:00:00 UTC... actually let's use a value near day boundary explicitly:
    time_t utc2300 = utcNoon + 11 * 3600; // 2026-01-15 23:00:00 UTC
    struct tm tNextDay = computeRemoteTm(utc2300, 2 * 3600); // +2h -> 01:00 next day
    CHECK_EQ(tNextDay.tm_hour, 1);
    CHECK_EQ(tNextDay.tm_mday, 16);

    TEST_SUMMARY();
}
