#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <Ticker.h>

// Include all modular headers
#include "constants.h"
#include "settings.h"
#include "sensor.h"
#include "display.h"
#include "wifi_config.h"
#include "webserver_config.h"
#include "ota.h"
#include "storage.h"
#include "time_utils.h"
#include "menu.h"
#include "input.h"

// Global time variable for all modules
unsigned long now;

// OTA timer
Ticker rebootTimer;

namespace {

const char* resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNMAPPED";
  }
}

}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] An error has occurred while mounting LittleFS");
    LOGE("[LittleFS] An error has occurred while mounting LittleFS");
  }
  Serial.println("[LittleFS] LittleFS mounted successfully");
  LOGI("[LittleFS] LittleFS mounted successfully");
}

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);

  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("[System] Reset reason: %d (%s)\n", (int)rr, resetReasonToString(rr));
  const String resetReasonMsg = "[System] Reset reason: " + String((int)rr) + " (" + String(resetReasonToString(rr)) + ")";

  const int bootSteps = 11; // Update if more steps are added to the boot sequence
  
  // Bring up hardware and persistent storage first, then load runtime settings.
  initTFT();
  showBootStatus("Init SD", 1, bootSteps);
  initSD();
  LOGI(resetReasonMsg);
  LOGI("Boot ESP32 : Hi ;-) Welcome ! ---------------");
  
  showBootStatus("Reglages", 2, bootSteps);
  loadSettings();

  showBootStatus("Init boutons", 3, bootSteps);
  inputInit();

  showBootStatus("Init WiFi", 4, bootSteps);
  initWifi();

  showBootStatus("Init capteur", 5, bootSteps);
  initSensorUart();

  showBootStatus("Monter FS", 6, bootSteps);
  initLittleFS();

  showBootStatus("Histo 7j", 7, bootSteps);
  loadLast7Days(last7Days, true);

  showBootStatus("Mesures x120", 8, bootSteps);
  loadLast120Measures();

  showBootStatus("Init prediction", 9, bootSteps);
  initPredictionFromHistory();
  
  // Menu starts at the root item so user always lands on a predictable UI state.
  showBootStatus("Init menu", 10, bootSteps);
  currentMenu = &mainMenu;
  selectedIndex = 1;
  actived = false;

  showBootStatus("Pret", 11, bootSteps);
  delay(1800);
}

// =================== LOOP ===================
void loop() {
  now = millis();
  
  // Main loop keeps each subsystem non-blocking and event driven.
  updateWifi();
  updateMeasure();
  updateAllButtonState();
  if (!isOTAOverlayActive()) {
    updateTFT(now);
  }
  updateWebSocket();
  updateOTA();
}
