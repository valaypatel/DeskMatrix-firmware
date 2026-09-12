#pragma once
#ifndef CW_CWDATETIME_H_INCLUDED
#define CW_CWDATETIME_H_INCLUDED
#include <Arduino.h>

// Adapted from Clockwise's CWDateTime (github.com/jnthas/clockwise,
// firmware/lib/cw-commons/CWDateTime.h). Upstream wraps the ezTime library
// and does its own NTP sync. DeskMatrix.ino already calls
// configTime(TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org") once in setup(), so
// this version just reads the C library's already-synced wall clock via
// time(nullptr)/localtime() instead of pulling in ezTime as a dependency.
// The public API surface (method names/signatures) is kept the same as
// upstream so the ported clockfaces (mario/words/pokedex) compile unchanged.
class CWDateTime
{
private:
  bool use24hFormat = true;

public:
  // timeZone/ntpServer/posixTZ are accepted for API compatibility with the
  // upstream signature but unused here — NTP sync and timezone offset are
  // already handled by DeskMatrix.ino's configTime() call before any
  // clockface is loaded.
  void begin(const char *timeZone, bool use24format, const char *ntpServer = "pool.ntp.org", const char *posixTZ = "");

  String getFormattedTime();
  String getFormattedTime(const char *format);

  // Both return a zero-padded 2-digit string (static internal buffer, like
  // upstream). The `format` argument is accepted for call-site compatibility
  // with the ported clockfaces but the output is always 2-digit zero-padded,
  // matching what every caller in this codebase actually wants.
  char *getHour(const char *format);
  char *getMinute(const char *format);

  int getHour();       // 0-23
  int getMinute();     // 0-59
  int getSecond();     // 0-59
  int getDay();        // 1-31 (day of month)
  int getMonth();      // 1-12
  int getYear();       // e.g. 2026
  int getWeekday();    // 0=Sunday .. 6=Saturday
  long getMilliseconds();
};

#endif  // CW_CWDATETIME_H_INCLUDED