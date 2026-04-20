#include "settings_schema.h"
//#include <cstddef>

namespace {

template <typename T>
void clampRangedValue(RangedValue<T>& ranged) {
  if (ranged.value < ranged.min) {
    ranged.value = ranged.min;
  }
  if (ranged.value > ranged.max) {
    ranged.value = ranged.max;
  }
}

void appendPresetChoices(JsonDocument& doc,
                         const char* group,
                         const char* key,
                         const PresetChoice* choices,
                         size_t count) {
  JsonArray out = doc[group][key]["choices"].to<JsonArray>();
  for (size_t i = 0; i < count; ++i) {
    JsonObject choice = out.add<JsonObject>();
    choice["value"] = choices[i].value;
    choice["label"] = choices[i].label;
  }
}

void appendTimeZoneChoices(JsonDocument& doc,
                           const char* group,
                           const char* key,
                           const TimeZoneChoice* choices,
                           size_t count) {
  JsonArray out = doc[group][key]["choices"].to<JsonArray>();
  for (size_t i = 0; i < count; ++i) {
    JsonObject choice = out.add<JsonObject>();
    choice["value"] = choices[i].tzString;
    choice["label"] = choices[i].label;
  }
}

} // namespace

const PresetChoice MEASURE_PERIOD_PRESETS[] = {
  {10000UL, "10 sec"},
  {30000UL, "30 sec"},
  {60000UL, "1 min"},
  {300000UL, "5 min"},
  {900000UL, "15 min"},
  {1800000UL, "30 min"},
  {3600000UL, "1 h"}
};

const PresetChoice PAGE_INTERVAL_PRESETS[] = {
  {1UL, "1 sec"},
  {2UL, "2 sec"},
  {5UL, "5 sec"},
  {8UL, "8 sec"},
  {10UL, "10 sec"},
  {15UL, "15 sec"},
  {30UL, "30 sec"}
};

const PresetChoice SENSOR_SAMPLE_METHOD_PRESETS[] = {
  {0UL, "Moyenne"},
  {1UL, "Mediane"}
};

const PresetChoice SENSOR_SAMPLE_INTERVAL_PRESETS[] = {
  {40UL, "40 ms"},
  {80UL, "80 ms"},
  {120UL, "120 ms"},
  {200UL, "200 ms"},
  {300UL, "300 ms"},
  {500UL, "500 ms"},
  {750UL, "750 ms"},
  {1000UL, "1 sec"},
  {1500UL, "1.5 sec"},
  {2000UL, "2 sec"}
};

const PresetChoice WIFI_RETRY_DELAY_PRESETS[] = {
  {1000UL, "1 sec"},
  {2000UL, "2 sec"},
  {5000UL, "5 sec"},
  {10000UL, "10 sec"},
  {30000UL, "30 sec"},
  {60000UL, "1 min"}
};

const PresetChoice DISPLAY_BRIGHTNESS_PRESETS[] = {
  {10UL, "10%"},
  {35UL, "20%"},
  {60UL, "30%"},
  {85UL, "40%"},
  {110UL, "50%"},
  {135UL, "60%"},
  {160UL, "70%"},
  {185UL, "80%"},
  {210UL, "90%"},
  {255UL, "100%"}
};

const TimeZoneChoice TIMEZONE_PRESETS[] = {
  {"CET-1CEST,M3.5.0,M10.5.0", "Europe/Zurich (CET/CEST)"},
  {"GMT0BST,M3.5.0,M10.5.0", "Europe/London (GMT/BST)"},
  {"WET0WEST,M3.5.0,M10.5.0", "Europe/Lisbon (WET/WEST)"},
  {"EET-2EEST,M3.5.0,M10.5.0", "Europe/Athens (EET/EEST)"},
  {"MIT-0", "UTC (GMT)"},
  {"CET-1", "Europe/Paris (hiver)"},
};

const PresetChoice SENSOR_PREDICTION_METHOD_PRESETS[] = {
  {-1UL, "None"},
  {0UL, "Moyenne"},
  {1UL, "Lagrange"},
  {2UL, "Spline cubique"},
  {3UL, "Mediane"}
};

const size_t SENSOR_PREDICTION_METHOD_PRESET_COUNT = sizeof(SENSOR_PREDICTION_METHOD_PRESETS) / sizeof(SENSOR_PREDICTION_METHOD_PRESETS[0]);

const size_t MEASURE_PERIOD_PRESET_COUNT = sizeof(MEASURE_PERIOD_PRESETS) / sizeof(MEASURE_PERIOD_PRESETS[0]);
const size_t PAGE_INTERVAL_PRESET_COUNT = sizeof(PAGE_INTERVAL_PRESETS) / sizeof(PAGE_INTERVAL_PRESETS[0]);
const size_t SENSOR_SAMPLE_METHOD_PRESET_COUNT = sizeof(SENSOR_SAMPLE_METHOD_PRESETS) / sizeof(SENSOR_SAMPLE_METHOD_PRESETS[0]);
const size_t SENSOR_SAMPLE_INTERVAL_PRESET_COUNT = sizeof(SENSOR_SAMPLE_INTERVAL_PRESETS) / sizeof(SENSOR_SAMPLE_INTERVAL_PRESETS[0]);
const size_t WIFI_RETRY_DELAY_PRESET_COUNT = sizeof(WIFI_RETRY_DELAY_PRESETS) / sizeof(WIFI_RETRY_DELAY_PRESETS[0]);
const size_t DISPLAY_BRIGHTNESS_PRESET_COUNT = sizeof(DISPLAY_BRIGHTNESS_PRESETS) / sizeof(DISPLAY_BRIGHTNESS_PRESETS[0]);
const size_t TIMEZONE_PRESET_COUNT = sizeof(TIMEZONE_PRESETS) / sizeof(TIMEZONE_PRESETS[0]);

String formatPresetLabel(unsigned long value,
                         const PresetChoice* presets,
                         size_t count,
                         const char* fallbackUnit) {
  for (size_t i = 0; i < count; ++i) {
    if (presets[i].value == value) {
      return String(presets[i].label);
    }
  }

  return String(value) + " " + fallbackUnit;
}

unsigned long clampToPresetRange(unsigned long value,
                                 const PresetChoice* presets,
                                 size_t count) {
  if (count == 0) {
    return value;
  }

  if (value <= presets[0].value) {
    return presets[0].value;
  }

  if (value >= presets[count - 1].value) {
    return presets[count - 1].value;
  }

  return value;
}

unsigned long normalizePresetValue(unsigned long value,
                                   const PresetChoice* presets,
                                   size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (presets[i].value == value) {
      return value;
    }
  }

  return clampToPresetRange(value, presets, count);
}

int normalizeBrightnessSettingValue(int value) {
  if (value < 0) {
    return static_cast<int>(DISPLAY_BRIGHTNESS_PRESETS[0].value);
  }

  return static_cast<int>(normalizePresetValue(static_cast<unsigned long>(value),
                                                DISPLAY_BRIGHTNESS_PRESETS,
                                                DISPLAY_BRIGHTNESS_PRESET_COUNT));
}

uint8_t getDisplayBrightnessPwm(int level) {
  return static_cast<uint8_t>(normalizeBrightnessSettingValue(level));
}

void normalizeSettingsValues(Settings& settingsRef) {
    settingsRef.mesure.predictionMethod.value = normalizePresetValue(
        settingsRef.mesure.predictionMethod.value,
        SENSOR_PREDICTION_METHOD_PRESETS,
        SENSOR_PREDICTION_METHOD_PRESET_COUNT);
  // Ranged values
  clampRangedValue(settingsRef.tank.capacite_L);
  clampRangedValue(settingsRef.tank.height_mm);
  clampRangedValue(settingsRef.sensor.offset_mm);
  clampRangedValue(settingsRef.mesure.sampleCount);
  clampRangedValue(settingsRef.sensor.spikeThreshold_mm);
  clampRangedValue(settingsRef.sensor.spikeRetryMax);
  clampRangedValue(settingsRef.sensor.predictionRetryMax);
  clampRangedValue(settingsRef.display.standbyInterval);
  clampRangedValue(settingsRef.button.debounceTime);
  clampRangedValue(settingsRef.button.longPressTime);

  // Preset values
  settingsRef.wifi.retryDelay.value = normalizePresetValue(
      settingsRef.wifi.retryDelay.value,
      WIFI_RETRY_DELAY_PRESETS,
      WIFI_RETRY_DELAY_PRESET_COUNT);

  settingsRef.mesure.measurePeriod.value = normalizePresetValue(
      settingsRef.mesure.measurePeriod.value,
      MEASURE_PERIOD_PRESETS,
      MEASURE_PERIOD_PRESET_COUNT);

  settingsRef.mesure.sampleInterval.value = normalizePresetValue(
      settingsRef.mesure.sampleInterval.value,
      SENSOR_SAMPLE_INTERVAL_PRESETS,
      SENSOR_SAMPLE_INTERVAL_PRESET_COUNT);

  settingsRef.mesure.sampleMethod.value = normalizePresetValue(
      settingsRef.mesure.sampleMethod.value,
      SENSOR_SAMPLE_METHOD_PRESETS,
      SENSOR_SAMPLE_METHOD_PRESET_COUNT);

  settingsRef.display.pageInterval.value = static_cast<int>(normalizePresetValue(
      static_cast<unsigned long>(settingsRef.display.pageInterval.value),
      PAGE_INTERVAL_PRESETS,
      PAGE_INTERVAL_PRESET_COUNT));

  settingsRef.display.bright.value = normalizeBrightnessSettingValue(settingsRef.display.bright.value);
  settingsRef.display.standbyBright.value = normalizeBrightnessSettingValue(settingsRef.display.standbyBright.value);

  // String value
  settingsRef.timeSource.tzString.trim();
  if (settingsRef.timeSource.tzString.length() == 0) {
    settingsRef.timeSource.tzString = "CET-1CEST,M3.5.0,M10.5.0";
  }
}

void exportSettingsToApiJson(const Settings& settingsRef, JsonDocument& doc) {
    doc["mesure"]["predictionMethod"]["value"] = settingsRef.mesure.predictionMethod.value;
    doc["mesure"]["predictionMethod"]["min"] = -1;
    doc["mesure"]["predictionMethod"]["max"] = 3;
    doc["mesure"]["predictionMethod"]["step"] = 1;
    appendPresetChoices(doc, "mesure", "predictionMethod", SENSOR_PREDICTION_METHOD_PRESETS, SENSOR_PREDICTION_METHOD_PRESET_COUNT);
  doc["tank"]["capacite_L"]["value"] = settingsRef.tank.capacite_L.value;
  doc["tank"]["capacite_L"]["min"] = settingsRef.tank.capacite_L.min;
  doc["tank"]["capacite_L"]["max"] = settingsRef.tank.capacite_L.max;
  doc["tank"]["capacite_L"]["step"] = settingsRef.tank.capacite_L.step;

  doc["tank"]["height_mm"]["value"] = settingsRef.tank.height_mm.value;
  doc["tank"]["height_mm"]["min"] = settingsRef.tank.height_mm.min;
  doc["tank"]["height_mm"]["max"] = settingsRef.tank.height_mm.max;
  doc["tank"]["height_mm"]["step"] = settingsRef.tank.height_mm.step;

  doc["wifi"]["enabled"] = settingsRef.wifi.enabled;
  doc["wifi"]["retryDelay"]["value"] = settingsRef.wifi.retryDelay.value;
  appendPresetChoices(doc, "wifi", "retryDelay", WIFI_RETRY_DELAY_PRESETS, WIFI_RETRY_DELAY_PRESET_COUNT);

  doc["display"]["bright"]["value"] = settingsRef.display.bright.value;
  appendPresetChoices(doc, "display", "bright", DISPLAY_BRIGHTNESS_PRESETS, DISPLAY_BRIGHTNESS_PRESET_COUNT);

  doc["display"]["standbyBright"]["value"] = settingsRef.display.standbyBright.value;
  appendPresetChoices(doc, "display", "standbyBright", DISPLAY_BRIGHTNESS_PRESETS, DISPLAY_BRIGHTNESS_PRESET_COUNT);

  doc["display"]["standbyInterval"]["value"] = settingsRef.display.standbyInterval.value;
  doc["display"]["standbyInterval"]["min"] = settingsRef.display.standbyInterval.min;
  doc["display"]["standbyInterval"]["max"] = settingsRef.display.standbyInterval.max;
  doc["display"]["standbyInterval"]["step"] = settingsRef.display.standbyInterval.step;
  doc["display"]["standbyActived"] = settingsRef.display.standbyActived;

  doc["display"]["pageInterval"]["value"] = settingsRef.display.pageInterval.value;
  appendPresetChoices(doc, "display", "pageInterval", PAGE_INTERVAL_PRESETS, PAGE_INTERVAL_PRESET_COUNT);

  doc["sensor"]["offset_mm"]["value"] = settingsRef.sensor.offset_mm.value;
  doc["sensor"]["offset_mm"]["min"] = settingsRef.sensor.offset_mm.min;
  doc["sensor"]["offset_mm"]["max"] = settingsRef.sensor.offset_mm.max;
  doc["sensor"]["offset_mm"]["step"] = settingsRef.sensor.offset_mm.step;

  doc["sensor"]["enableSpikeDetection"] = settingsRef.sensor.enableSpikeDetection;
  doc["sensor"]["spikeRetryMax"]["value"] = settingsRef.sensor.spikeRetryMax.value;
  doc["sensor"]["spikeRetryMax"]["min"] = settingsRef.sensor.spikeRetryMax.min;
  doc["sensor"]["spikeRetryMax"]["max"] = settingsRef.sensor.spikeRetryMax.max;
  doc["sensor"]["spikeRetryMax"]["step"] = settingsRef.sensor.spikeRetryMax.step;
  
  doc["sensor"]["enableIqrFilter"] = settingsRef.sensor.enableIqrFilter;

  doc["sensor"]["predictionRetryMax"]["value"] = settingsRef.sensor.predictionRetryMax.value;
  doc["sensor"]["predictionRetryMax"]["min"] = settingsRef.sensor.predictionRetryMax.min;
  doc["sensor"]["predictionRetryMax"]["max"] = settingsRef.sensor.predictionRetryMax.max;
  doc["sensor"]["predictionRetryMax"]["step"] = settingsRef.sensor.predictionRetryMax.step;

  doc["mesure"]["sampleCount"]["value"] = settingsRef.mesure.sampleCount.value;
  doc["mesure"]["sampleCount"]["min"] = settingsRef.mesure.sampleCount.min;
  doc["mesure"]["sampleCount"]["max"] = settingsRef.mesure.sampleCount.max;
  doc["mesure"]["sampleCount"]["step"] = settingsRef.mesure.sampleCount.step;

  doc["sensor"]["spikeThreshold_mm"]["value"] = settingsRef.sensor.spikeThreshold_mm.value;
  doc["sensor"]["spikeThreshold_mm"]["min"] = settingsRef.sensor.spikeThreshold_mm.min;
  doc["sensor"]["spikeThreshold_mm"]["max"] = settingsRef.sensor.spikeThreshold_mm.max;
  doc["sensor"]["spikeThreshold_mm"]["step"] = settingsRef.sensor.spikeThreshold_mm.step;

  // expose retry settings in storage API
  doc["sensor"]["spikeRetryMax"]["value"] = settingsRef.sensor.spikeRetryMax.value;
  doc["sensor"]["predictionRetryMax"]["value"] = settingsRef.sensor.predictionRetryMax.value;



  doc["mesure"]["sampleMethod"]["value"] = settingsRef.mesure.sampleMethod.value;
  doc["mesure"]["sampleMethod"]["min"] = 0;
  doc["mesure"]["sampleMethod"]["max"] = 1;
  doc["mesure"]["sampleMethod"]["step"] = 1;
  appendPresetChoices(doc, "mesure", "sampleMethod", SENSOR_SAMPLE_METHOD_PRESETS, SENSOR_SAMPLE_METHOD_PRESET_COUNT);

  doc["mesure"]["sampleInterval"]["value"] = settingsRef.mesure.sampleInterval.value;
  appendPresetChoices(doc, "mesure", "sampleInterval", SENSOR_SAMPLE_INTERVAL_PRESETS, SENSOR_SAMPLE_INTERVAL_PRESET_COUNT);

  doc["mesure"]["measurePeriod"]["value"] = settingsRef.mesure.measurePeriod.value;
  appendPresetChoices(doc, "mesure", "measurePeriod", MEASURE_PERIOD_PRESETS, MEASURE_PERIOD_PRESET_COUNT);

  doc["button"]["debounceTime"]["value"] = settingsRef.button.debounceTime.value;
  doc["button"]["debounceTime"]["min"] = settingsRef.button.debounceTime.min;
  doc["button"]["debounceTime"]["max"] = settingsRef.button.debounceTime.max;
  doc["button"]["debounceTime"]["step"] = settingsRef.button.debounceTime.step;

  doc["button"]["longPressTime"]["value"] = settingsRef.button.longPressTime.value;
  doc["button"]["longPressTime"]["min"] = settingsRef.button.longPressTime.min;
  doc["button"]["longPressTime"]["max"] = settingsRef.button.longPressTime.max;
  doc["button"]["longPressTime"]["step"] = settingsRef.button.longPressTime.step;

  doc["timeSource"]["tzString"]["value"] = settingsRef.timeSource.tzString;
  appendTimeZoneChoices(doc, "timeSource", "tzString", TIMEZONE_PRESETS, TIMEZONE_PRESET_COUNT);
}

void exportSettingsToStorageJson(const Settings& settingsRef, JsonDocument& doc) {
  doc["sensor"]["enableSpikeDetection"] = settingsRef.sensor.enableSpikeDetection;
  doc["mesure"]["predictionMethod"]["value"] = settingsRef.mesure.predictionMethod.value;
  doc["tank"]["capacite_L"]["value"] = settingsRef.tank.capacite_L.value;
  doc["tank"]["height_mm"]["value"] = settingsRef.tank.height_mm.value;
  doc["wifi"]["enabled"] = settingsRef.wifi.enabled;
  doc["wifi"]["retryDelay"]["value"] = settingsRef.wifi.retryDelay.value;

  doc["display"]["bright"]["value"] = settingsRef.display.bright.value;
  doc["display"]["standbyBright"]["value"] = settingsRef.display.standbyBright.value;
  doc["display"]["standbyInterval"]["value"] = settingsRef.display.standbyInterval.value;
  doc["display"]["standbyActived"] = settingsRef.display.standbyActived;
  doc["display"]["pageInterval"]["value"] = settingsRef.display.pageInterval.value;

  doc["sensor"]["offset_mm"]["value"] = settingsRef.sensor.offset_mm.value;
  doc["sensor"]["spikeThreshold_mm"]["value"] = settingsRef.sensor.spikeThreshold_mm.value;
  doc["sensor"]["spikeRetryMax"]["value"] = settingsRef.sensor.spikeRetryMax.value;
  doc["sensor"]["predictionRetryMax"]["value"] = settingsRef.sensor.predictionRetryMax.value;
  doc["sensor"]["enableIqrFilter"] = settingsRef.sensor.enableIqrFilter;
  doc["mesure"]["sampleCount"]["value"] = settingsRef.mesure.sampleCount.value;
  doc["mesure"]["sampleMethod"]["value"] = settingsRef.mesure.sampleMethod.value;
  doc["mesure"]["sampleInterval"]["value"] = settingsRef.mesure.sampleInterval.value;

  doc["mesure"]["measurePeriod"]["value"] = settingsRef.mesure.measurePeriod.value;

  doc["button"]["debounceTime"]["value"] = settingsRef.button.debounceTime.value;
  doc["button"]["longPressTime"]["value"] = settingsRef.button.longPressTime.value;

  doc["timeSource"]["tzString"] = settingsRef.timeSource.tzString;
}

void applySettingsFromJson(Settings& settingsRef, const JsonDocument& doc) {
  settingsRef.sensor.enableSpikeDetection = doc["sensor"]["enableSpikeDetection"] | settingsRef.sensor.enableSpikeDetection;
  settingsRef.tank.capacite_L.value = doc["tank"]["capacite_L"]["value"] | settingsRef.tank.capacite_L.value;
  settingsRef.tank.height_mm.value = doc["tank"]["height_mm"]["value"] | settingsRef.tank.height_mm.value;
  
  settingsRef.wifi.enabled = doc["wifi"]["enabled"] | settingsRef.wifi.enabled;
  settingsRef.wifi.retryDelay.value = doc["wifi"]["retryDelay"]["value"] | settingsRef.wifi.retryDelay.value;

  settingsRef.display.bright.value = doc["display"]["bright"]["value"] | settingsRef.display.bright.value;
  settingsRef.display.standbyBright.value = doc["display"]["standbyBright"]["value"] | settingsRef.display.standbyBright.value;
  settingsRef.display.standbyInterval.value = doc["display"]["standbyInterval"]["value"] | settingsRef.display.standbyInterval.value;
  settingsRef.display.standbyActived = doc["display"]["standbyActived"] | settingsRef.display.standbyActived;
  settingsRef.display.pageInterval.value = doc["display"]["pageInterval"]["value"] | settingsRef.display.pageInterval.value;

  settingsRef.sensor.offset_mm.value = doc["sensor"]["offset_mm"]["value"] | settingsRef.sensor.offset_mm.value;
  settingsRef.sensor.spikeThreshold_mm.value = doc["sensor"]["spikeThreshold_mm"]["value"] | settingsRef.sensor.spikeThreshold_mm.value;
  settingsRef.sensor.spikeRetryMax.value = doc["sensor"]["spikeRetryMax"]["value"] | settingsRef.sensor.spikeRetryMax.value;
  settingsRef.sensor.predictionRetryMax.value = doc["sensor"]["predictionRetryMax"]["value"] | settingsRef.sensor.predictionRetryMax.value;
  settingsRef.sensor.enableIqrFilter = doc["sensor"]["enableIqrFilter"] | settingsRef.sensor.enableIqrFilter;

  settingsRef.mesure.sampleCount.value = doc["mesure"]["sampleCount"]["value"] | settingsRef.mesure.sampleCount.value;
  settingsRef.mesure.sampleMethod.value = doc["mesure"]["sampleMethod"]["value"] | settingsRef.mesure.sampleMethod.value;
  settingsRef.mesure.sampleInterval.value = doc["mesure"]["sampleInterval"]["value"] | settingsRef.mesure.sampleInterval.value;
  settingsRef.mesure.predictionMethod.value = doc["mesure"]["predictionMethod"]["value"] | settingsRef.mesure.predictionMethod.value;
  settingsRef.mesure.measurePeriod.value = doc["mesure"]["measurePeriod"]["value"] | settingsRef.mesure.measurePeriod.value;

  settingsRef.button.debounceTime.value = doc["button"]["debounceTime"]["value"] | settingsRef.button.debounceTime.value;
  settingsRef.button.longPressTime.value = doc["button"]["longPressTime"]["value"] | settingsRef.button.longPressTime.value;

  JsonVariantConst tzNode = doc["timeSource"]["tzString"];
  if (tzNode.is<JsonObjectConst>()) {
    settingsRef.timeSource.tzString = tzNode["value"] | settingsRef.timeSource.tzString;
  } else {
    settingsRef.timeSource.tzString = tzNode | settingsRef.timeSource.tzString;
  }

  normalizeSettingsValues(settingsRef);
}
