#include "time_utils.h"
#include "settings.h"
#include <time.h>

const char* ntpServer = "pool.ntp.org";

void initNTP_Time() {
  // Use timezone string with automatic DST support
  // The ESP32 will handle DST transitions automatically
  configTzTime(settings.timeSource.tzString.c_str(), ntpServer);

  // Optional startup check: keeps logs explicit when clock is not yet synced.
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[NTP] Failed to get local time");
  } else {
    Serial.printf("[NTP] Local time set: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
}

String getCurrentDateTime() {
  time_t nowEpoch = time(nullptr);
  // Guard against invalid RTC/NTP state (epoch still close to 1970).
  if (nowEpoch < 1600000000) {
    return "--/--/---- --:--:--";
  }

  struct tm timeinfo;
  localtime_r(&nowEpoch, &timeinfo);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

bool getCurrentWeekday(int& weekday) {
  time_t nowEpoch = time(nullptr);
  // Same guard used by graph labels to avoid fake weekday values.
  if (nowEpoch < 1600000000) {
    return false;
  }

  struct tm timeinfo;
  localtime_r(&nowEpoch, &timeinfo);
  weekday = timeinfo.tm_wday;
  return true;
}
