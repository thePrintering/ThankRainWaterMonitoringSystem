
#ifndef PREDICTION_H
#define PREDICTION_H

#include <Arduino.h>
#include <math.h>
#include "constants.h"

class PredictionModule {
public:
    PredictionModule();
    void add(float value);
    float predictLagrange();
    float predictMean(int N = 5);
    float predictSplineCubique();
    float predictMedian();
    int count() const { return historyCount; }
    float last() const;
    String formatHistoryLog() const;
private:
    float history[PREDICTION_HISTORY_SIZE];
    int historyIndex;
    int historyCount;
};

#endif // PREDICTION_H
