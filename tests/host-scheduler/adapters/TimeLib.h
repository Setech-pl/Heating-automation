#pragma once
#include "platform.h"
extern int fakeHour, fakeMinute;
inline int hour() { return fakeHour; }
inline int minute() { return fakeMinute; }
inline int weekday() { return 5; }
inline int day() { return 8; }
inline int month() { return 10; }
