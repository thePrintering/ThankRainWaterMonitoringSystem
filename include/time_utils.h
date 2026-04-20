#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

// =================== NTP CONFIG ===================
extern const char* ntpServer;

// =================== TIME CONFIGURATION ===================
// Uses POSIX timezone strings with automatic DST support.
// The system will automatically switch between standard time and daylight saving time.
// Current setting is defined in settings.timeSource.tzString
// Examples of timezone strings:
//   "CET-1CEST,M3.5.0,M10.5.0"  - Central Europe (UTC+1/+2)
//   "GMT0BST,M3.5.0,M10.5.0"    - UK/Ireland (UTC+0/+1)
//   "EST5EDT,M3.2.0,M11.1.0"    - US Eastern (UTC-5/-4)
//   "CST6CDT,M3.2.0,M11.1.0"    - US Central (UTC-6/-5)

// =================== FUNCTION DECLARATIONS ===================
void initNTP_Time();
String getCurrentDateTime();
bool getCurrentWeekday(int& weekday);

#endif // TIME_UTILS_H
