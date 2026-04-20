#include <Arduino.h>
#include "prediction.h"
#include <vector>

PredictionModule::PredictionModule() : historyIndex(0), historyCount(0) {
    for (int i = 0; i < PREDICTION_HISTORY_SIZE; ++i) history[i] = 0;
}

void PredictionModule::add(float value) {
    history[historyIndex] = value;
    historyIndex = (historyIndex + 1) % PREDICTION_HISTORY_SIZE;
    if (historyCount < PREDICTION_HISTORY_SIZE) historyCount++;
}

float PredictionModule::last() const {
    if (historyCount == 0) return 0;
    int idx = (historyIndex - 1 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    return history[idx];
}

float PredictionModule::predictLagrange() {
    if (historyCount < 3) return last();
    int i2 = (historyIndex - 1 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    int i1 = (historyIndex - 2 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    int i0 = (historyIndex - 3 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    float x0 = -2, x1 = -1, x2 = 0;
    float y0 = history[i0];
    float y1 = history[i1];
    float y2 = history[i2];
    float x = 1; // prédire le prochain point
    float L0 = ((x-x1)*(x-x2))/((x0-x1)*(x0-x2));
    float L1 = ((x-x0)*(x-x2))/((x1-x0)*(x1-x2));
    float L2 = ((x-x0)*(x-x1))/((x2-x0)*(x2-x1));
    return y0*L0 + y1*L1 + y2*L2;
}

float PredictionModule::predictMean(int N) {
    if (historyCount == 0) return 0;
    N = (N > historyCount) ? historyCount : N;
    float sum = 0;
    for (int i = 0; i < N; ++i) {
        int idx = (historyIndex - 1 - i + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
        sum += history[idx];
    }
    return sum / N;
}

float PredictionModule::predictMedian() {
    if (historyCount == 0) return 0;
    // Copie l'historique dans un tableau temporaire
    int n = historyCount;
    float temp[PREDICTION_HISTORY_SIZE];
    for (int i = 0; i < n; ++i) {
        int idx = (historyIndex - 1 - i + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
        temp[i] = history[idx];
    }
    std::vector<float> sorted(temp, temp + n);
    std::sort(sorted.begin(), sorted.end());
    if (n % 2 == 1) {
        return sorted[n / 2];
    } else {
        return 0.5f * (sorted[n / 2 - 1] + sorted[n / 2]);
    }
}

// Interpolation spline cubique simple (Hermite, 4 points)
float PredictionModule::predictSplineCubique() {
    if (historyCount < 4) return last();
    // Indices des 4 derniers points
    int i3 = (historyIndex - 1 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    int i2 = (historyIndex - 2 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    int i1 = (historyIndex - 3 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    int i0 = (historyIndex - 4 + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
    float y0 = history[i0];
    float y1 = history[i1];
    float y2 = history[i2];
    float y3 = history[i3];
    // Spline cubique de Catmull-Rom (t=1)
    float t = 1.0f; // prédire le prochain point
    float t2 = t * t;
    float t3 = t2 * t;
    float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    float a1 = y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    float a2 = -0.5f*y0 + 0.5f*y2;
    float a3 = y1;
    return a0*t3 + a1*t2 + a2*t + a3;
}


String PredictionModule::formatHistoryLog() const {
    String histLog = "[";
    for (int i = 0; i < historyCount; ++i) {
        int idx = (historyIndex - 1 - i + PREDICTION_HISTORY_SIZE) % PREDICTION_HISTORY_SIZE;
        histLog += String(history[idx]);
        if (i < historyCount - 1) histLog += ",";
    }
    histLog += "]";
    return histLog;
}
