#pragma once
#include "platform.h"
inline void setTime(time_t value) {
  ++platform.setTimeCalls;
  platform.clock = value;
  platform.clockValid = true;
}
inline int hour() { return 12; }
inline int minute() { return 34; }
inline int weekday() { return 5; }
inline int day() { return 8; }
inline int month() { return 10; }
