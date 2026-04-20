#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include "constants.h"
#include "sensor.h"

// =================== DISPLAY OBJECTS ===================
extern TFT_eSPI tft;
extern TFT_eSprite sprite;

extern DisplayPage page;
extern DisplayMode mode;
extern int wifiQualityRSSI;

// =================== DISPLAY TIMING VARIABLES ===================
extern long int lastPressButtonTime;
extern unsigned long lastPageSwitch;

// =================== FUNCTION DECLARATIONS ===================
void initTFT();
void updateTFT(unsigned long now);
void showBootStatus(const String& status, int step = -1, int totalSteps = -1);

// Display pages
void drawGaugePage();
void drawGraphPage();
void drawDataPage();
void drawInfoPage();
void drawMenuPage();
void drawPageNumber(DisplayPage page);

// Utilities
void drawWiFiArcsIcone(int x, int y, int wifiQualityRSSI, uint16_t colorA = TFT_WHITE, uint16_t colorB = TFT_DARKGREY, uint16_t bg = 0);
void drawWaterLevelSensorStatusIcon(int x, int y, bool connected, uint16_t colorA = TFT_WHITE, uint16_t colorB = TFT_DARKGREY, uint16_t bg = 0);
void drawProgressBarOTA(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint8_t percentage, uint16_t frameColor, uint16_t barColor);
void showRestartConfirmationScreen();
void showRestartScreen();
void showFactoryResetConfirmationScreen();
void showFactoryResetScreen();
void showAPConfirmationScreen();
void showAPCredentialsScreen(const String &ssid, const String &password);
void updateAPConnectionStatus(int connectedClients);
void updateAPCountdown(unsigned long remainingSeconds);
void updateAPWiFiStatus();

// Page switching
void switchPageForward();
void switchPageBack();

#endif // DISPLAY_H
