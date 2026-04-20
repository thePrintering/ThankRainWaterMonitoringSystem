#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "sensor.h"

// =================== GLOBAL VARIABLES ===================
extern bool sdOK;

// =================== FUNCTION DECLARATIONS ===================
void initSD();
bool ensureSDReady();
bool sdExists(const String &path);
bool removeSDFile(const String &path);
String getSDCardTypeName();
uint64_t getSDTotalBytes();
uint64_t getSDUsedBytes();
void writeHistoryToSD(const SensorData &data);
String getDailyLogFile();
void logSD(const String& msg);
SensorData computeDailyAverage(const char *path);
void loadLast7Days(DailyAverageBuffer &buf, bool infoLog = false);
void loadLast120Measures();
String buildMonthAggregateJsonFromMonthlyFile(int year, int month);
String buildYearAggregateJsonFromMonthlyFiles(int year);
bool rebuildMonthlySummaryForDailyFile(const String &dailyFileName);
void pruneDailyHistoryDirectoriesForFile(const String &dailyFileName);

#endif // STORAGE_H
