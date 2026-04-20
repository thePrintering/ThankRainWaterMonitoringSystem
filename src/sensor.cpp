#include "prediction.h"
static PredictionModule predictionModule;
// Calcule la moyenne d'un tableau de long
long computeMeanSample(const long* arr, int count) {
  if (!arr || count <= 0) return 0;
  long sum = 0;
  for (int i = 0; i < count; ++i) sum += arr[i];
  return sum / count;
}
#include "sensor.h"
#include "settings.h"
#include "constants.h"
#include "storage.h"
#include <Arduino.h>
#include <algorithm>

// =================== GLOBAL VARIABLES ===================
UltrasonicState ultrasonicState;
SensorData sensorData;
DailyAverageBuffer last7Days;

float distanceBuffer[FILTER_SAMPLES];
int bufferIndex = 0;
bool bufferFilled = false;

SensorData history[HISTORY_SIZE];
int historyIndex = 0;
bool historyFilled = false;

unsigned long lastUpdateTime = 0;

// UART communication
HardwareSerial sensorSerial(2);
uint8_t CS;
unsigned char buffer_RTT[4] = {0};

static bool ensureSampleBufferCapacity(UltrasonicState& state, size_t desiredCapacity) {
  if (desiredCapacity == 0) {
    desiredCapacity = 1;
  }

  if (state.samples && state.samplesCapacity == desiredCapacity) {
    return true;
  }

  long* newSamples = new (std::nothrow) long[desiredCapacity]();
  if (!newSamples) {
    return false;
  }

  delete[] state.samples;
  state.samples = newSamples;
  state.samplesCapacity = desiredCapacity;
  return true;
}

static void clearSampleBuffer(UltrasonicState& state) {
  if (!state.samples || state.samplesCapacity == 0) {
    return;
  }

  for (size_t i = 0; i < state.samplesCapacity; ++i) {
    state.samples[i] = 0;
  }
}

static String formatCollectedSamples(const UltrasonicState& state, int count) {
  if (!state.samples || state.samplesCapacity == 0 || count <= 0) {
    return "[]";
  }

  const int boundedCount = min(count, static_cast<int>(state.samplesCapacity));
  String out = "[";
  for (int i = 0; i < boundedCount; ++i) {
    if (i > 0) {
      out += ",";
    }
    out += String(state.samples[i]);
  }
  out += "]";
  return out;
}

// =================== SENSOR IMPLEMENTATIONS ===================

// Filtre les outliers selon la méthode IQR (Q1, Q3, borne min/max)
// Retourne le nombre de valeurs filtrées (copiées dans filtered[])
int filterOutliersByIQR(const long* samples, int count, long* filtered) {
  if (count <= 0) return 0;
  std::vector<long> sorted(samples, samples + count);
  std::sort(sorted.begin(), sorted.end());
  int q1_idx = count / 4;
  int q3_idx = (3 * count) / 4;
  long Q1 = sorted[q1_idx];
  long Q3 = sorted[q3_idx];
  long IQR = Q3 - Q1;
  long minVal = Q1 - 1.5 * IQR;
  long maxVal = Q3 + 1.5 * IQR;
  int filteredCount = 0;
  for (int i = 0; i < count; ++i) {
    if (samples[i] >= minVal && samples[i] <= maxVal) {
      filtered[filteredCount++] = samples[i];
    }
  }
  return filteredCount;
}

void DailyAverageBuffer::addMeasurement(SensorData value) {
  // Rotate daily slot automatically when calendar day changes.
  checkNewDay();

  todaySum.distance += value.distance;
  todaySum.percent += value.percent;
  todaySum.volume += value.volume;
  todayCount++;

  SensorData avg;
  avg.distance = todaySum.distance / todayCount;
  avg.percent = todaySum.percent / todayCount;
  avg.volume = todaySum.volume / todayCount;

  int todayIndex = (index + 6) % 7;
  SensorDataBuffer[todayIndex] = avg;
}

void DailyAverageBuffer::checkNewDay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  int currentDay = timeinfo.tm_mday;

  if (lastDay == -1) {
    lastDay = currentDay;
    return;
  }

  if (currentDay != lastDay) {
    // Finalize yesterday averages before accumulating the new day.
    finalizeDay();
    lastDay = currentDay;
  }
}

void DailyAverageBuffer::finalizeDay() {
  if (todayCount > 0) {
    SensorData avg;
    avg.distance = todaySum.distance / todayCount;
    avg.percent = todaySum.percent / todayCount;
    avg.volume = todaySum.volume / todayCount;
    SensorDataBuffer[index] = avg;

    index = (index + 1) % 7;

    if (count < 7) {
      count++;
    }
  }

  todaySum.distance = 0;
  todaySum.percent = 0;
  todaySum.volume = 0;
  todayCount = 0;
}

void initSensorUart() {
  sensorSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  ultrasonicState.sensorState = 1;
}

int readUltrasonicsensorUART() {
  int Distance;
  // Flush stale bytes before sending a new command frame.
  while (sensorSerial.available()) sensorSerial.read();
  sensorSerial.write(COM_ULTRASONIC);
  delay(100);
  int avail = sensorSerial.available();
#ifdef SENSOR_UART_DEBUG
  LOGI(String("[SENSOR][UART] available=") + String(avail));
#endif

  if (avail >= 4) {
    int first = sensorSerial.read();
    if (first == 0xFF) {
      buffer_RTT[0] = 0xFF;
      for (int i = 1; i < 4; i++) {
        int b = sensorSerial.read();
        buffer_RTT[i] = static_cast<unsigned char>(b & 0xFF);
      }

      CS = buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2];

#ifdef SENSOR_UART_DEBUG
      char hexBuf[128];
      snprintf(hexBuf, sizeof(hexBuf), "%02X %02X %02X %02X cs_rcv=%02X cs_calc=%02X",
               buffer_RTT[0], buffer_RTT[1], buffer_RTT[2], buffer_RTT[3], buffer_RTT[3], CS);
      LOGI(String("[SENSOR][UART] ") + String(hexBuf));
#endif

      if (buffer_RTT[3] == CS) {
#ifdef SENSOR_UART_DEBUG
        LOGI(String("[SENSOR][UART] distance=") + String((buffer_RTT[1] << 8) | buffer_RTT[2]));
#endif
        Distance = (buffer_RTT[1] << 8) | buffer_RTT[2];
        return Distance;
      } else {
#ifdef SENSOR_UART_DEBUG
        LOGW(String("[SENSOR][UART] checksum mismatch"));
#endif
      }
    } else {
#ifdef SENSOR_UART_DEBUG
      char fbuf[32];
      snprintf(fbuf, sizeof(fbuf), "unexpected first byte=0x%02X", first & 0xFF);
      LOGW(String("[SENSOR][UART] ") + String(fbuf));
#endif
    }
  }

  return -1;
}

// Calcule la médiane d'un tableau de long (non modifiant)
long computeMedianSample(const long* arr, int count) {
  if (!arr || count <= 0) return 0;
  std::vector<long> sorted(arr, arr + count);
  std::sort(sorted.begin(), sorted.end());
  if ((count % 2) == 1) {
    return sorted[count / 2];
  }
  const long left = sorted[(count / 2) - 1];
  const long right = sorted[count / 2];
  return (left + right) / 2;
}

// Version pour UltrasonicState (pour compatibilité existante)
long computeMedianSample(const UltrasonicState& state, int count) {
  if (!state.samples || state.samplesCapacity == 0) return 0;
  int boundedCount = min(count, static_cast<int>(state.samplesCapacity));
  return computeMedianSample(state.samples, boundedCount);
}

int updateUltrasonic(UltrasonicState &state, SensorData &data, unsigned long now) {

  // Keep the cycle idle until the configured period elapses, except at first boot.
  if (now - state.lastFullMeasure < settings.mesure.measurePeriod.value && !state.firstMeasure) {
    return ANY_MESURE_READY;
  }

  if (now - state.lastSampleTime >= settings.mesure.sampleInterval.value || state.firstMeasure) {
    state.lastSampleTime = now;

    if (!ensureSampleBufferCapacity(state, static_cast<size_t>(settings.mesure.sampleCount.value))) {
      return MESURE_ISSUS;
    }

    int value = readUltrasonicsensorUART();
    if (value >= 0) {
      state.sum += value;
      if (state.sampleCount < static_cast<int>(state.samplesCapacity)) {
        state.samples[state.sampleCount] = value;
      }
      state.sampleCount++;
    } else {
      return MESURE_ISSUS;
    }

    if (state.sampleCount >= static_cast<int>(settings.mesure.sampleCount.value)) {

      const unsigned long collectedSampleCount = max(1UL, static_cast<unsigned long>(state.sampleCount));
      long rawDistance = 0;
      if (settings.sensor.enableIqrFilter) {
        // Filtrage des outliers par IQR (Q1, Q3)
        long filteredSamples[20]; // max 20 samples, ajuster si besoin
        int filteredCount = 0;
        {
          // Log des bornes IQR et des valeurs filtrées
          std::vector<long> sorted(state.samples, state.samples + static_cast<int>(collectedSampleCount));
          std::sort(sorted.begin(), sorted.end());
          int q1_idx = static_cast<int>(collectedSampleCount) / 4;
          int q3_idx = (3 * static_cast<int>(collectedSampleCount)) / 4;
          long Q1 = sorted[q1_idx];
          long Q3 = sorted[q3_idx];
          long IQR = Q3 - Q1;
          long minVal = Q1 - 1.5 * IQR;
          long maxVal = Q3 + 1.5 * IQR;
          filteredCount = filterOutliersByIQR(state.samples, static_cast<int>(collectedSampleCount), filteredSamples);
          String allSamplesLog = formatCollectedSamples(state, static_cast<int>(collectedSampleCount));
          String filteredSamplesLog = "[";
          for (int i = 0; i < filteredCount; ++i) {
            if (i > 0) filteredSamplesLog += ",";
            filteredSamplesLog += String(filteredSamples[i]);
          }
          filteredSamplesLog += "]";
          LOGI("[SENSOR][IQR] Q1=" + String(Q1) + " Q3=" + String(Q3) + " IQR=" + String(IQR) +
               " min=" + String(minVal) + " max=" + String(maxVal) +
               " allSamples=" + allSamplesLog +
               " filteredSamples=" + filteredSamplesLog);
        }
        if (filteredCount > 0) {
          if (settings.mesure.sampleMethod.value == 1) {
            // Calculer la médiane sur les valeurs filtrées
            rawDistance = computeMedianSample(filteredSamples, filteredCount);
          } else {
            // Moyenne sur les valeurs filtrées
            rawDistance = computeMeanSample(filteredSamples, filteredCount);
          }
        } else {
          // fallback : médiane/moyenne classique si tout a été filtré
          if (settings.mesure.sampleMethod.value == 1) {
            rawDistance = computeMedianSample(state, static_cast<int>(collectedSampleCount));
          } else {
            rawDistance = state.sum / collectedSampleCount;
          }
        }
      } else {
        // Pas de filtrage IQR : médiane/moyenne classique
        if (settings.mesure.sampleMethod.value == 1) {
          rawDistance = computeMedianSample(state, static_cast<int>(collectedSampleCount));
        } else {
          rawDistance = state.sum / collectedSampleCount;
        }
      }

      const String samplesLog = formatCollectedSamples(state, static_cast<int>(collectedSampleCount));
      const char* methodLabel = (settings.mesure.sampleMethod.value == 1) ? "median" : "average";

      // Sélection de la méthode de prédiction selon settings
    float predicted = 0;
    switch (settings.mesure.predictionMethod.value) {
      case -1:
        predicted = 0; // No prediction, use raw distance for comparison
        break;
      case 0:
        predicted = predictionModule.predictMean(5);
        break;
      case 1:
        predicted = predictionModule.predictLagrange();
        break;
      case 2:
        predicted = predictionModule.predictSplineCubique();
        break;
      case 3:
        predicted = predictionModule.predictMedian();
        break;
      default:
        predicted = predictionModule.predictLagrange();
        break;
    }

    float deltaPred = fabs(rawDistance - predicted);
    float seuilPred = 0.30 * fabs(predicted); // seuil à 30% de la prédiction
    // Calcul des autres prédictions pour comparaison
    float predMean = predictionModule.predictMean(5);
    float predLagrange = predictionModule.predictLagrange();
    float predSpline = predictionModule.predictSplineCubique();
    float predMedian = predictionModule.predictMedian();

    String predMethod;
    switch (settings.mesure.predictionMethod.value) {
      case -1: predMethod = "none"; break;
      case 0: predMethod = "mean"; break;
      case 1: predMethod = "lagrange"; break;
      case 2: predMethod = "spline"; break;
      case 3: predMethod = "median"; break;
      default: predMethod = "?"; break;
    }
    String histLog = predictionModule.formatHistoryLog();
    
    if (predictionModule.count() >= 3 && deltaPred > seuilPred && settings.mesure.predictionMethod.value != -1) {
      // If under the allowed retry budget, increment and schedule a retry.
      if (state.predictionRetryCount < static_cast<int>(settings.sensor.predictionRetryMax.value)) {
        state.predictionRetryCount++;
        LOGW(String("[SENSOR][PREDICT] Mesure trop éloignée de la prédiction (retry ") + String(state.predictionRetryCount) + "/" + String(settings.sensor.predictionRetryMax.value) + "): raw=" + String(rawDistance) +
             " pred=" + String(predicted) +
             " delta=" + String(deltaPred) +
             " seuil=" + String(seuilPred) +
             " method=" + predMethod +
             " hist=" + histLog +
             " | mean=" + String(predMean) +
             " lagrange=" + String(predLagrange) +
             " spline=" + String(predSpline) +
             " median=" + String(predMedian));
        // Reset sample state but force a delay before retry
        state.sampleCount = 0;
        state.sum = 0;
        clearSampleBuffer(state);
        state.lastSampleTime = now + SENSOR_SPIKE_RETRY_DELAY_MS;  // Wait before next retry
        return ANY_MESURE_READY;
      } else {
        // Exceeded retries: accept the measurement anyway (override prediction).
        LOGW(String("[SENSOR][PREDICT] Max prediction retries reached, accepting measurement: raw=") + String(rawDistance) +
             " pred=" + String(predicted) +
             " delta=" + String(deltaPred) +
             " seuil=" + String(seuilPred) +
             " method=" + predMethod +
             " hist=" + histLog +
             " | mean=" + String(predMean) +
             " lagrange=" + String(predLagrange) +
             " spline=" + String(predSpline) +
             " median=" + String(predMedian));
        // fall through to accept the measure
      }
    }
    if(settings.mesure.predictionMethod.value != -1) {
      LOGI("[SENSOR][PREDICT] raw=" + String(rawDistance) +
          " pred=" + String(predicted) +
          " delta=" + String(deltaPred) +
          " seuil=" + String(seuilPred) +
          " method=" + predMethod +
          " hist=" + histLog +
          " | mean=" + String(predMean) +
          " lagrange=" + String(predLagrange) +
          " spline=" + String(predSpline) +
          " median=" + String(predMedian));
    }
      if (state.lastAcceptedRawDistance >= 0) {
        long delta = rawDistance - state.lastAcceptedRawDistance;
        if (delta < 0) {
          delta = -delta;
        }

        if (settings.sensor.enableSpikeDetection) {
          if (delta > static_cast<long>(settings.sensor.spikeThreshold_mm.value) && state.spikeRetryCount < static_cast<int>(settings.sensor.spikeRetryMax.value)) {
            state.spikeRetryCount++;
            LOGW("[SENSOR] Spike detecte -> reessai " + String(state.spikeRetryCount) + "/" + String(settings.sensor.spikeRetryMax.value)
                 + " raw=" + String(rawDistance)
                 + " prev=" + String(state.lastAcceptedRawDistance)
                 + " delta=" + String(delta)
                 + " seuil=" + String(settings.sensor.spikeThreshold_mm.value)
                 + " method=" + String(methodLabel)
                 + " n=" + String(collectedSampleCount)
                 + " samples=" + samplesLog);
            // Reset sample state but force a delay before retry
            state.sampleCount = 0;
            state.sum = 0;
            clearSampleBuffer(state);
            state.lastSampleTime = now + SENSOR_SPIKE_RETRY_DELAY_MS;  // Wait before next retry
            return ANY_MESURE_READY;
          }

          if (delta > static_cast<long>(settings.sensor.spikeThreshold_mm.value)) {
            LOGW("[SENSOR] Spike persistant accepte apres reessaies"
                 + String(" raw=") + String(rawDistance)
                 + " prev=" + String(state.lastAcceptedRawDistance)
                 + " delta=" + String(delta)
                 + " seuil=" + String(settings.sensor.spikeThreshold_mm.value)
                 + " method=" + String(methodLabel)
                 + " n=" + String(collectedSampleCount)
                 + " samples=" + samplesLog);
          }
        }
      }

      float distance = rawDistance;
      data.distance = distance - settings.sensor.offset_mm.value;
      distance = constrain(data.distance, 0, settings.tank.height_mm.value);

      float waterLevel = settings.tank.height_mm.value - distance;
      data.percent = constrain((waterLevel / settings.tank.height_mm.value) * 100.0, 0, 100);
      data.volume = (data.percent / 100.0) * settings.tank.capacite_L.value;

      LOGI("[SENSOR] Mesure valide"
         + String(" raw=") + String(rawDistance)
         + " distanceCorr=" + String(data.distance, 2)
         + " percent=" + String(data.percent, 2)
         + " volume=" + String(data.volume, 2)
         + " method=" + String(methodLabel)
         + " n=" + String(collectedSampleCount)
         + " reessaies=" + String(state.spikeRetryCount)
         + " samples=" + samplesLog);
      
      // Ajoute la dernière mesure validée à l'historique de prédiction (avant de calculer la nouvelle)
      if (rawDistance > 0) {
        predictionModule.add(rawDistance);
      }

      state.sampleCount = 0;
      state.sum = 0;
      clearSampleBuffer(state);
      state.lastAcceptedRawDistance = rawDistance;
      state.spikeRetryCount = 0;
      state.predictionRetryCount = 0;
      state.lastFullMeasure = now;
      state.firstMeasure = 0;
      return MESURE_READY;
    }
  }
  return ANY_MESURE_READY;
}

void addToHistory(SensorData &data) {
  // Ring buffer: newest item overwrites oldest when full.
  history[historyIndex] = data;

  historyIndex++;
  if (historyIndex >= HISTORY_SIZE) {
    historyIndex = 0;
    historyFilled = true;
  }
}

void initPredictionFromHistory() {
  // Initialize predictionModule from persisted history (oldest -> newest)
  int count = historyFilled ? HISTORY_SIZE : historyIndex;
  if (count <= 0) {
    return;
  }

  int start = (count > PREDICTION_HISTORY_SIZE) ? (count - PREDICTION_HISTORY_SIZE) : 0;
  for (int i = start; i < count; ++i) {
    float corrDist = history[i].distance;
    if (corrDist <= 0.0f) continue;
    // Rebuild raw distance stored previously before offset correction
    float rawDist = corrDist + settings.sensor.offset_mm.value;
    predictionModule.add(rawDist);
  }
  LOGI("[PREDICT] Initialised from SD history: " + predictionModule.formatHistoryLog());
  Serial.println("[PREDICT] Initialised from SD history: " + predictionModule.formatHistoryLog());
}

void resetMeasureCycle(UltrasonicState &state, bool resetSpikeRetryCount) {
  // Reset the measurement cycle after settings/time changes.
  state.sampleCount = 0;
  state.sum = 0;
  if (resetSpikeRetryCount) {
    state.spikeRetryCount = 0;
    state.predictionRetryCount = 0;
  }
  clearSampleBuffer(state);
  state.lastSampleTime = 0;
  state.firstMeasure = true;
}

void updateMeasure() {
  extern String getCurrentDateTime();
  extern void writeHistoryToSD(const SensorData &data);
  extern void notifyWebSocketClients(SensorData &data);
  extern unsigned long now;
  extern AsyncWebSocket webSocket;

  switch (updateUltrasonic(ultrasonicState, sensorData, now)) {
    case MESURE_READY: {
      ultrasonicState.sensorState = 1;
      sensorData.time = getCurrentDateTime();
      Serial.println(sensorData.time);

      // Persist and broadcast only when a complete validated measure is ready.
      addToHistory(sensorData);
      writeHistoryToSD(sensorData);
      lastUpdateTime = now;

      // Keep weekly graph aligned with full on-SD daily averages, including today's updated mean.
      loadLast7Days(last7Days);

      if (webSocket.count()) {
        notifyWebSocketClients(sensorData);
      }
    }
    break;
    case ANY_MESURE_READY:
      break;
    case MESURE_ISSUS: {
      ultrasonicState.sensorState = 0;
      resetMeasureCycle(ultrasonicState);
    }
    break;
  }
}
