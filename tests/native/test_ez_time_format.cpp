// tests/native/test_ez_time_format.cpp
#include "test_framework.h"
#include "EzTimeFormat.h"

int main() {
    // "H:i" -- the format every real Canvas theme uses for its clock digits
    CHECK_EQ(formatEzTime("H:i", 9, 5, 30, 12, 3, 2026), "09:05");
    CHECK_EQ(formatEzTime("H:i", 23, 59, 0, 1, 1, 2026), "23:59");

    // "m-d" -- the date-line format seen in the real Nyan Cat theme
    CHECK_EQ(formatEzTime("m-d", 9, 5, 30, 3, 12, 2026), "12-03");

    // 12-hour tokens
    CHECK_EQ(formatEzTime("h:i", 0, 5, 0, 1, 1, 2026), "12:05");   // midnight -> 12
    CHECK_EQ(formatEzTime("h:i", 13, 5, 0, 1, 1, 2026), "01:05");  // 1pm -> 01, padded
    CHECK_EQ(formatEzTime("g:i", 13, 5, 0, 1, 1, 2026), "1:05");   // 1pm -> 1, unpadded

    // Unpadded day/month
    CHECK_EQ(formatEzTime("j/n", 9, 5, 30, 3, 12, 2026), "3/12");

    // Year tokens
    CHECK_EQ(formatEzTime("Y", 9, 5, 30, 3, 12, 2026), "2026");
    CHECK_EQ(formatEzTime("y", 9, 5, 30, 3, 12, 2026), "26");

    // Unknown characters (separators) pass through unchanged
    CHECK_EQ(formatEzTime("H:i:s", 9, 5, 30, 3, 12, 2026), "09:05:30");

    TEST_SUMMARY();
}
