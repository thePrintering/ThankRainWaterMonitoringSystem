#include "display.h"
#include "constants.h"

// TFT display object and sprite (offscreen buffer for flicker-free rendering)
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// Current display page and operating mode
DisplayPage page = GAUGE_PAGE_NUM;
DisplayMode mode = AUTO_MODE_NUM;

extern unsigned long now;
extern unsigned long lastUpdateTime;

// Initialize the TFT display
// - Sets rotation, creates sprite buffer
// - Configures brightness PWM (GPIO 38, channel 0)
// - Shows welcome screen
void initTFT() {
  tft.init();
  tft.setRotation(0);

  sprite.createSprite(SCREEN_W, SCREEN_H);
  sprite.setSwapBytes(true);

  ledcSetup(0, 10000, 8);
  ledcAttachPin(38, 0);
  ledcWrite(0, 255);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("ESP32 Rain");
  tft.setCursor(10, 40);
  tft.println("Tank Monitor");
  tft.setTextSize(1);
  tft.setCursor(10, 80);
  tft.print("FW ");
  tft.println(VERSION);

  showBootStatus("Init TFT", -1, -1);
}

void showBootStatus(const String& status, int step, int totalSteps) {
  const int logX = 8;
  const int logY = 118;
  const int logW = SCREEN_W - 16;
  const int logH = 140;
  const int lineH = 10;
  const int maxLines = 6;
  const int barX = 10;
  const int barY = 270;
  const int barW = SCREEN_W - 20;
  const int barH = 12;

  static String bootLog[maxLines];
  static int bootLogCount = 0;

  String line = status;
  if (step >= 0 && totalSteps > 0) {
    line = "[" + String(step) + "/" + String(totalSteps) + "] " + status;
  }

  if (bootLogCount < maxLines) {
    bootLog[bootLogCount++] = line;
  } else {
    for (int i = 1; i < maxLines; i++) {
      bootLog[i - 1] = bootLog[i];
    }
    bootLog[maxLines - 1] = line;
  }

  tft.fillRect(0, logY - 14, SCREEN_W, logH + 20, TFT_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(logX, logY - 12);
  tft.print("Boot log:");

  tft.drawRect(logX - 2, logY - 2, logW + 4, logH, TFT_DARKGREY);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  for (int i = 0; i < bootLogCount; i++) {
    tft.setCursor(logX, logY + i * lineH);
    tft.println(bootLog[i]);
  }

  if (step >= 0 && totalSteps > 0) {
    int safeStep = constrain(step, 0, totalSteps);
    int fillW = map(safeStep, 0, totalSteps, 0, barW - 2);

    tft.drawRect(barX, barY, barW, barH, TFT_DARKGREY);
    tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, TFT_GREEN);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 288);
    tft.printf("%d/%d", safeStep, totalSteps);
  }
}