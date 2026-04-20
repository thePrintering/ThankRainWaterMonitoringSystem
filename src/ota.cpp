#include <ArduinoOTA.h>

#include "ota.h"
#include "display.h"
#include "storage.h"
#include "input.h"

namespace {

constexpr int OTA_BAR_X = 10;
constexpr int OTA_BAR_Y = 92;
constexpr int OTA_BAR_W = 150;
constexpr int OTA_BAR_H = 18;

int lastRenderedPercent = -1;
bool otaOverlayActive = false;
bool otaErrorActive = false;
bool otaErrorWaitRelease = false;

void drawOTABaseScreen(const String &type) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(5, 16);
  tft.println("Mise a jour");
  tft.setTextSize(1);
  tft.setCursor(5, 44);
  tft.print("Type: ");
  tft.println(type);
  tft.setCursor(5, 58);
  tft.println("Ne pas eteindre l'appareil");
  drawProgressBarOTA(OTA_BAR_X, OTA_BAR_Y, OTA_BAR_W, OTA_BAR_H, 0, TFT_DARKGREY, TFT_BLUE);
}

void drawOTAProgress(int percentage) {
  if (percentage == lastRenderedPercent) {
    return;
  }

  lastRenderedPercent = percentage;
  drawProgressBarOTA(OTA_BAR_X, OTA_BAR_Y, OTA_BAR_W, OTA_BAR_H, percentage, TFT_WHITE, TFT_BLUE);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextPadding(tft.textWidth("100%"));
  tft.setCursor(10, 120);
  tft.printf("%d%%", percentage);
}

void drawOTAErrorScreen(ota_error_t error) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 16);
  tft.println("OTA ERREUR");

  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.print("Code: ");
  tft.println((int)error);
  tft.setCursor(10, 66);

  if (error == OTA_AUTH_ERROR)
    tft.println("Auth Failed");
  else if (error == OTA_BEGIN_ERROR)
    tft.println("Begin Failed");
  else if (error == OTA_CONNECT_ERROR)
    tft.println("Connect Failed");
  else if (error == OTA_RECEIVE_ERROR)
    tft.println("Receive Failed");
  else if (error == OTA_END_ERROR)
    tft.println("End Failed");
  else
    tft.println("Unknown Error");

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 90);
  tft.println("Appuyer un bouton");
  tft.setCursor(10, 104);
  tft.println("pour fermer");
}

bool processOTAErrorAcknowledge() {
  if (!otaErrorActive) {
    return false;
  }

  if (otaErrorWaitRelease) {
    if (!isAnyUserButtonPressed()) {
      otaErrorWaitRelease = false;
    } else {
      return false;
    }
  }

  if (isAnyUserButtonPressed()) {
    // Debounce the acknowledge press without blocking long.
    delay(60);
    otaErrorActive = false;
    otaOverlayActive = false;
    return true;
  }

  return false;
}

void drawOTARebootCountdown() {
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 162);
  tft.println("Redemarrage...");

  tft.setTextSize(2);
  for (int sec = 3; sec >= 1; sec--) {
    tft.fillRect(10, 178, 140, 26, TFT_BLACK);
    tft.setCursor(10, 182);
    tft.printf("%d", sec);
    delay(1000);
  }
}

} // namespace


void initOTA() {
  // Register OTA callbacks once after WiFi services start.
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";

      lastRenderedPercent = -1;
      Serial.println("[OTA] Start updating " + type);
      LOGI("[OTA] Start updating " + type);

      // Force full brightness while update UI is shown.
      ledcWrite(0, 255);
      otaOverlayActive = true;
      otaErrorActive = false;
      drawOTABaseScreen(type);
    })
    .onEnd([]() {
      drawOTAProgress(100);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setTextSize(1);
      tft.setCursor(10, 146);
      tft.println("Mise a jour terminee");
      drawOTARebootCountdown();
      otaOverlayActive = false;
      otaErrorActive = false;
      Serial.println("\n[OTA] End");
      LOGI("[OTA] End");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      // Keep screen feedback responsive during long uploads.
      if (total == 0) {
        return;
      }

      int percentage = (int)((progress * 100U) / total);
      percentage = constrain(percentage, 0, 100);
      Serial.printf("[OTA] Progress: %d%%\r", percentage);
      drawOTAProgress(percentage);
    })
    .onError([](ota_error_t error) {
      // Print human-readable error directly on display for field debugging.
      Serial.printf("[OTA] OTA Error[%u]\n", error);
      LOGE("[OTA] OTA Error[" + String(error) + "]");
      drawOTAErrorScreen(error);
      otaOverlayActive = true;
      otaErrorActive = true;
      otaErrorWaitRelease = isAnyUserButtonPressed();
    });

  ArduinoOTA.setPort(3232);
  ArduinoOTA.begin();
  Serial.println("[OTA] Starting OTA");
  LOGI("[OTA] Starting OTA");
}

void updateOTA() {
  // Handle OTA packets in the main loop without blocking other modules.
  ArduinoOTA.handle();

  if (otaErrorActive) {
    const bool acknowledged = processOTAErrorAcknowledge();
    if (acknowledged) {
      LOGI("[OTA] Erreur OTA confirmee via bouton utilisateur");
    }
  }

  delay(0);
  yield();
}

bool isOTAOverlayActive() {
  return otaOverlayActive;
}
