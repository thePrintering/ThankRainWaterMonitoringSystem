#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// =================== VALUE TEMPLATES ===================
template <typename T>
struct RangedValue {
  T value;
  T min;
  T max;
  T step;
};

template <typename T>
struct PresetValue {
  T value;
};

using MaxMinStepIntValue = RangedValue<int>;
using MaxMinStepFloatValue = RangedValue<float>;
using MaxMinStepULongValue = RangedValue<unsigned long>;


// =================== TANK SETTINGS ===================
struct TankSettings {
  MaxMinStepFloatValue capacite_L = {5000, 0, 100000, 100}; // Tank capacity in liters
  MaxMinStepFloatValue height_mm = {1030.0, 0, 100000, 1};  // Tank height in mm
};

// =================== SENSOR SETTINGS ===================
struct SensorSettings {
  MaxMinStepFloatValue offset_mm = {390.0, 0, 1000, 1}; // Sensor offset in mm
  bool enableSpikeDetection = true; // Enable spike detection
  MaxMinStepULongValue spikeThreshold_mm = {60, 10, 300, 5}; // Spike filter threshold
  // Max retries for spike detection (previously compile-time constant)
  MaxMinStepULongValue spikeRetryMax = {4, 0, 20, 1};
  // Max retries for prediction rejection (new runtime setting)
  MaxMinStepULongValue predictionRetryMax = {4, 0, 20, 1};
  bool enableIqrFilter = false; // Enable IQR outlier filter
};

// =================== MEASUREMENT SETTINGS ===================
struct MesureSettings {
  MaxMinStepULongValue sampleCount = {5, 1, 100, 1};      // Number of samples per measure
  PresetValue<unsigned long> sampleMethod = {1}; // 0 = average, 1 = median
  PresetValue<unsigned long> predictionMethod = {2}; // -1 = none, 0 = moyenne, 1 = Lagrange, 2 = spline cubique, 3 = médiane
  PresetValue<unsigned long> sampleInterval = {500}; // ms between samples
  PresetValue<unsigned long> measurePeriod = {1800000}; // ms between measurements
};

// =================== DISPLAY SETTINGS ===================
struct DisplaySettings {
  MaxMinStepIntValue standbyInterval = {10, 1, 60, 2}; // min before standby
  bool standbyActived = true;                          // standby mode enabled
  PresetValue<int> pageInterval = {8};                 // seconds per page
  PresetValue<int> bright = {255};                     // 100% brightness
  PresetValue<int> standbyBright = {10};               // standby brightness
};

// =================== BUTTON SETTINGS ===================
struct ButtonSettings {
  MaxMinStepIntValue debounceTime = {40, 10, 1000, 10};    // ms debounce
  MaxMinStepIntValue longPressTime = {2000, 1000, 4000, 500}; // ms for long press
};

// =================== WIFI SETTINGS ===================
struct WifiSettings {
  bool enabled = true;
  PresetValue<unsigned long> retryDelay = {5000}; // ms between retries
};

// =================== TIME SOURCE SETTINGS ===================
struct TimeSourceSettings {
  // POSIX timezone string with automatic DST support
  // Examples:
  // Europe (CET/CEST): "CET-1CEST,M3.5.0,M10.5.0"
  // UK (GMT/BST):      "GMT0BST,M3.5.0,M10.5.0"
  // US Eastern (EST/EDT): "EST5EDT,M3.2.0,M11.1.0"
  // US Central (CST/CDT): "CST6CDT,M3.2.0,M11.1.0"
  // Default: Europe timezone
  String tzString = "CET-1CEST,M3.5.0,M10.5.0";
};

// =================== MASTER SETTINGS CONTAINER ===================
struct Settings {
  TankSettings tank;
  SensorSettings sensor;
  MesureSettings mesure;
  DisplaySettings display;
  ButtonSettings button;
  WifiSettings wifi;
  TimeSourceSettings timeSource;
};

extern Settings settings;

void loadSettings();
void saveSettings();

#endif // SETTINGS_H
