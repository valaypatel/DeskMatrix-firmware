#include "lib/cw-commons/CWDateTime.h"
#include <time.h>

void CWDateTime::begin(const char *timeZone, bool use24format, const char *ntpServer, const char *posixTZ)
{
  // NTP sync + timezone are already established by DeskMatrix.ino's
  // configTime() call in setup(), before any clockface is loaded — nothing
  // to do here except remember the 12h/24h preference.
  (void)timeZone;
  (void)ntpServer;
  (void)posixTZ;
  this->use24hFormat = use24format;
}

static struct tm getLocalTm()
{
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return timeinfo;
}

String CWDateTime::getFormattedTime()
{
  return getFormattedTime(use24hFormat ? "%H:%M:%S" : "%I:%M:%S");
}

String CWDateTime::getFormattedTime(const char *format)
{
  struct tm timeinfo = getLocalTm();
  char buffer[32];
  strftime(buffer, sizeof(buffer), format, &timeinfo);
  return String(buffer);
}

char *CWDateTime::getHour(const char *format)
{
  (void)format;
  static char buffer[3] = {'\0'};
  snprintf(buffer, sizeof(buffer), "%02d", getHour());
  return buffer;
}

char *CWDateTime::getMinute(const char *format)
{
  (void)format;
  static char buffer[3] = {'\0'};
  snprintf(buffer, sizeof(buffer), "%02d", getMinute());
  return buffer;
}

int CWDateTime::getHour()
{
  return getLocalTm().tm_hour;
}

int CWDateTime::getMinute()
{
  return getLocalTm().tm_min;
}

int CWDateTime::getSecond()
{
  return getLocalTm().tm_sec;
}

int CWDateTime::getDay()
{
  return getLocalTm().tm_mday;
}

int CWDateTime::getMonth()
{
  return getLocalTm().tm_mon + 1;
}

int CWDateTime::getWeekday()
{
  return getLocalTm().tm_wday;
}

long CWDateTime::getMilliseconds()
{
  return (long)millis();
}
