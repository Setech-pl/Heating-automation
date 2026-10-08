#pragma once
#include "platform.h"
enum timeStatus_t { timeNotSet, timeNeedsSync, timeSet };
struct tmElements_t { uint8_t Second, Minute, Hour, Wday, Day, Month, Year; };
#define SECS_PER_MIN 60UL
#define SECS_PER_HOUR 3600UL
#define SECS_PER_DAY 86400UL
#define SECS_PER_WEEK 604800UL
#ifdef HEATING_TEST_SCHEDULE_CLOCK
extern int fakeHour, fakeMinute;
#endif
inline time_t now() {
  if (platform.clockValid) return platform.clock;
#ifdef HEATING_TEST_SCHEDULE_CLOCK
  return (platform.calendarEpoch / 86400) * 86400 + fakeHour * 3600 + fakeMinute * 60;
#else
  return platform.calendarEpoch;
#endif
}
inline void setTime(time_t value) {
  ++platform.setTimeCalls;
  platform.clock = value;
  platform.clockValid = true;
  platform.calendarValid = true;
}
inline timeStatus_t timeStatus() { return platform.calendarValid ? timeSet : timeNotSet; }
inline void breakTime(time_t value, tmElements_t &out) {
  std::tm converted = *std::gmtime(&value); // UTC conversion only, no wall-clock access.
  out = {static_cast<uint8_t>(converted.tm_sec), static_cast<uint8_t>(converted.tm_min),
         static_cast<uint8_t>(converted.tm_hour), static_cast<uint8_t>(converted.tm_wday + 1),
         static_cast<uint8_t>(converted.tm_mday), static_cast<uint8_t>(converted.tm_mon + 1),
         static_cast<uint8_t>(converted.tm_year - 70)};
}
inline time_t makeTime(tmElements_t &parts) {
  std::tm converted = {};
  converted.tm_sec = parts.Second; converted.tm_min = parts.Minute;
  converted.tm_hour = parts.Hour; converted.tm_mday = parts.Day;
  converted.tm_mon = parts.Month - 1; converted.tm_year = parts.Year + 70;
  return timegm(&converted);
}
inline int hour(time_t value) { tmElements_t parts; breakTime(value, parts); return parts.Hour; }
inline int minute(time_t value) { tmElements_t parts; breakTime(value, parts); return parts.Minute; }
inline int weekday(time_t value) { tmElements_t parts; breakTime(value, parts); return parts.Wday; }
inline int day(time_t value) { tmElements_t parts; breakTime(value, parts); return parts.Day; }
inline int month(time_t value) { tmElements_t parts; breakTime(value, parts); return parts.Month; }
inline int hour() { return hour(now()); }
inline int minute() { return minute(now()); }
inline int weekday() { return weekday(now()); }
inline int day() { return day(now()); }
inline int month() { return month(now()); }
