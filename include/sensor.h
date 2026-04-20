#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "settings.h"
#include <ESPAsyncWebServer.h>

// =================== SENSOR DATA STRUCTURES ===================
struct SensorData {
  float distance;
  float percent;
  float volume;
  String time;
};

struct UltrasonicState {
  int sampleCount = 0;
  long sum = 0;
  long* samples = nullptr;
  size_t samplesCapacity = 0;
  long lastAcceptedRawDistance = -1;
  uint8_t spikeRetryCount = 0;
  uint8_t predictionRetryCount = 0;
  unsigned long lastSampleTime = 0;
  unsigned long lastFullMeasure = 0;
  bool sensorState = 1;
  bool firstMeasure = false;
};

// =================== DAILY AVERAGE BUFFER ===================
struct DailyAverageBuffer {
  SensorData SensorDataBuffer[7] = {0};
  int index = 0;
  int count = 0;

  SensorData todaySum = {};
  int todayCount = 0;
  int lastDay = -1;

  void addMeasurement(SensorData value);
  void checkNewDay();
  void finalizeDay();
};

// =================== GLOBAL VARIABLES ===================
extern UltrasonicState ultrasonicState;
extern SensorData sensorData;
extern DailyAverageBuffer last7Days;

extern float distanceBuffer[5];
extern int bufferIndex;
extern bool bufferFilled;

extern SensorData history[120];
extern int historyIndex;
extern bool historyFilled;

extern unsigned long lastUpdateTime;

// =================== FUNCTION DECLARATIONS ===================
void initSensorUart();
int readUltrasonicsensorUART();
int updateUltrasonic(UltrasonicState &state, SensorData &data, unsigned long now);
void updateMeasure();
void addToHistory(SensorData &data);
void resetMeasureCycle(UltrasonicState &state, bool resetSpikeRetryCount = true);
void initPredictionFromHistory();

#endif // SENSOR_H
