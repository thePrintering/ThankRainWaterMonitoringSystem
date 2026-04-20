#ifndef SETTINGS_SCHEMA_H
#define SETTINGS_SCHEMA_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "settings.h"

struct PresetChoice {
  unsigned long value;
  const char* label;
};

struct TimeZoneChoice {
  const char* tzString;
  const char* label;
};

extern const PresetChoice MEASURE_PERIOD_PRESETS[];
extern const PresetChoice PAGE_INTERVAL_PRESETS[];
extern const PresetChoice SENSOR_SAMPLE_METHOD_PRESETS[];
extern const PresetChoice SENSOR_PREDICTION_METHOD_PRESETS[];
extern const PresetChoice SENSOR_SAMPLE_INTERVAL_PRESETS[];
extern const PresetChoice WIFI_RETRY_DELAY_PRESETS[];
extern const PresetChoice DISPLAY_BRIGHTNESS_PRESETS[];
extern const TimeZoneChoice TIMEZONE_PRESETS[];

extern const size_t MEASURE_PERIOD_PRESET_COUNT;
extern const size_t PAGE_INTERVAL_PRESET_COUNT;
extern const size_t SENSOR_SAMPLE_METHOD_PRESET_COUNT;
extern const size_t SENSOR_PREDICTION_METHOD_PRESET_COUNT;
extern const size_t SENSOR_SAMPLE_INTERVAL_PRESET_COUNT;
extern const size_t WIFI_RETRY_DELAY_PRESET_COUNT;
extern const size_t DISPLAY_BRIGHTNESS_PRESET_COUNT;
extern const size_t TIMEZONE_PRESET_COUNT;

String formatPresetLabel(unsigned long value,
                         const PresetChoice* presets,
                         size_t count,
                         const char* fallbackUnit);

unsigned long clampToPresetRange(unsigned long value,
                                 const PresetChoice* presets,
                                 size_t count);

unsigned long normalizePresetValue(unsigned long value,
                                   const PresetChoice* presets,
                                   size_t count);

int normalizeBrightnessSettingValue(int value);
uint8_t getDisplayBrightnessPwm(int level);

void normalizeSettingsValues(Settings& settingsRef);
void exportSettingsToApiJson(const Settings& settingsRef, JsonDocument& doc);
void exportSettingsToStorageJson(const Settings& settingsRef, JsonDocument& doc);
void applySettingsFromJson(Settings& settingsRef, const JsonDocument& doc);

#endif // SETTINGS_SCHEMA_H
