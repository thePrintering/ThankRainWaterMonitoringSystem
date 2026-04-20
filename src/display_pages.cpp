#include "display.h"
#include "sensor.h"
#include "wifi_config.h"
#include "time_utils.h"
#include "constants.h"
#include "settings.h"
#include "menu.h"
#include "storage.h"

// Draw the gauge/vertical bar page
// Shows fill percentage with vertical gauge, digital %, and 3D tank graphic
void drawGaugePage() {
  extern unsigned long lastUpdateTime;
  extern String getCurrentDateTime();

  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(10, 10);
  sprite.println("Niveau de");
  sprite.setCursor(10, 30);
  sprite.println("remplissage");

  sprite.drawRect(5, 5, 160, 310, TFT_DARKGREY);

  drawWiFiArcsIcone(140, 5, wifiQualityRSSI);
  drawWaterLevelSensorStatusIcon(140, 25, ultrasonicState.sensorState);

  int gaugeX = 20;
  int gaugeY = 70;
  int gaugeW = 30;
  int gaugeH = 200;

  for (int i = 0; i <= 4; i++) {
    int y = gaugeY + i * (gaugeH / 4);
    sprite.drawLine(gaugeX + gaugeW + 3, y, gaugeX + gaugeW + 12, y, TFT_GREEN);
  }

  int fillH = map(sensorData.percent, 0, 100, 0, gaugeH);
  sprite.fillRect(gaugeX + 1, gaugeY + gaugeH - fillH, gaugeW - 2, fillH, TFT_BLUE);

  sprite.drawRect(gaugeX, gaugeY, gaugeW, gaugeH, TFT_WHITE);

  sprite.setTextSize(3);
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setCursor(70, 120);
  sprite.print(int(sensorData.percent));
  sprite.print("%");

  int tankX = 70;
  int tankY = 180;
  int tankW = 70;
  int tankH = 60;

  int waterH = map(sensorData.percent, 0, 100, 0, tankH - 4);
  sprite.fillRect(tankX + 2, tankY + tankH - 2 - waterH, tankW - 4, waterH, TFT_BLUE);

  int surfaceY = tankY + tankH - 2 - waterH;
  sprite.drawLine(tankX + 2, surfaceY, tankX + tankW - 3, surfaceY, TFT_CYAN);

  sprite.drawRoundRect(tankX, tankY, tankW, tankH, 8, TFT_WHITE);

  sprite.fillCircle(tankX + tankW / 2, tankY - 10, 4, TFT_CYAN);
  sprite.drawLine(tankX + tankW / 2, tankY - 6, tankX + tankW / 2, tankY, TFT_CYAN);

  sprite.setTextColor(TFT_BLUE, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(tankX, tankY + tankH + 20);
  sprite.println("" + String(int(sensorData.volume)) + " L");

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(1);
  sprite.setCursor(10, 300);
  String lastMeasureTime = "--:--";
  if (sensorData.time.length() >= 16 && sensorData.time.charAt(10) == ' ') {
    lastMeasureTime = sensorData.time.substring(11, 16);
  }
  sprite.print("Derniere mesure: ");
  sprite.println(lastMeasureTime);

  sprite.pushSprite(0, 0);
}

// Draw historical data graph page
// Shows fill % history over the past 7 days as a line graph
void drawGraphPage() {
  sprite.fillSprite(TFT_BLACK);

  sprite.drawRect(5, 5, 160, 310, TFT_DARKGREY);

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(10, 10);
  sprite.println("Historique");
  sprite.setCursor(10, 30);
  sprite.println("7 jours");

  drawWiFiArcsIcone(140, 5, wifiQualityRSSI);
  drawWaterLevelSensorStatusIcon(140, 25, ultrasonicState.sensorState);

  int gx = 15;
  int gy = 70;
  int gw = 140;
  int gh = 200;
  int graphTopPadding = 6;

  //sprite.drawRect(gx, gy, gw, gh, TFT_WHITE);
  sprite.drawLine(gx, gy, gx, gh, TFT_WHITE);
  sprite.drawLine(gx, gy+gh, gw, gy+gh, TFT_WHITE);

  sprite.drawLine(gx, gy, gx, gy + gh, TFT_DARKGREY);
  sprite.drawLine(gx, gy + gh, gx + gw, gy + gh, TFT_DARKGREY);

  for (int i = 0; i <= 4; i++) {
    int y = gy + i * ((gh-graphTopPadding) / 4);
    sprite.drawLine(gx - 3, y, gx + 3, y, TFT_GREEN);
    sprite.setTextSize(1);
    sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    sprite.setCursor(0, y - 4);
    sprite.print(100 - i * 25);
  }

  for (int i = 0; i < 7 - 1; i++) {
    int idx1 = (last7Days.index + i) % 7;
    int idx2 = (last7Days.index + i + 1) % 7;

    int x1 = map(i, 0, 6, gx + 1, gx + gw - 2);
    int y1 = map(last7Days.SensorDataBuffer[idx1].percent, 0, 100, gy + gh - 2, gy + graphTopPadding);

    int x2 = map(i + 1, 0, 6, gx + 1, gx + gw - 2);
    int y2 = map(last7Days.SensorDataBuffer[idx2].percent, 0, 100, gy + gh - 2, gy + graphTopPadding);

    sprite.drawLine(x1, y1, x2, y2, TFT_CYAN);
    sprite.drawLine(x1, y1 - 1, x2, y2 - 1, TFT_CYAN);
    sprite.drawLine(x1, y1 + 1, x2, y2 + 1, TFT_CYAN);
  }

  const char* dayAbbrev[7] = {"Di", "Lu", "Ma", "Me", "Je", "Ve", "Sa"};
  int todayWday = 0;
  bool timeIsValid = getCurrentWeekday(todayWday);

  for (int i = 0; i < 7; i++) {
    int x = map(i, 0, 6, gx + 1, gx + gw - 2);
    String dayLabel;

    if (timeIsValid) {
      int daysAgo = 6 - i;
      int targetWday = (todayWday - daysAgo + 7) % 7;
      dayLabel = dayAbbrev[targetWday];
    } else {
      dayLabel = "J-" + String(6 - i);
    }

    int labelWidth = dayLabel.length() * 6;
    sprite.setCursor(x - (labelWidth / 2), gy + gh + 5);
    sprite.print(dayLabel);
  }

  sprite.setTextSize(1);
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setCursor(gx+gw-60, gy + gh + 15);
  sprite.print("<- 7 jours");

  

  sprite.pushSprite(0, 0);
}

// Draw data summary page
// Displays level, volume, WiFi, sensor, and SD card status
void drawDataPage() {
  sprite.fillSprite(TFT_BLACK);

  sprite.drawRect(5, 5, 160, 310, TFT_DARKGREY);

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(10, 10);
  sprite.println("Infos");

  drawWiFiArcsIcone(140, 5, wifiQualityRSSI);
  drawWaterLevelSensorStatusIcon(140, 25, ultrasonicState.sensorState);

  const int percentRounded = constrain(static_cast<int>(sensorData.percent + 0.5f), 0, 100);
  const unsigned long volumeRounded = static_cast<unsigned long>(sensorData.volume + 0.5f);

  sprite.setTextSize(2);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.setCursor(10, 50);
  sprite.println("Niveau:");
  sprite.setCursor(25, 70);
  sprite.print(percentRounded);
  sprite.println("%");

  sprite.setTextColor(TFT_CYAN, TFT_BLACK);
  sprite.setCursor(10, 98);
  sprite.println("Volume:");
  sprite.setCursor(25, 118);
  sprite.print(volumeRounded);
  sprite.println(" L");

  // Keep all runtime statuses in one column with size=2 for readability on-device.
  sprite.setTextSize(2);
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setCursor(10, 150);
  sprite.print("WiFi:");
  sprite.setTextColor(getWiFiColor(), TFT_BLACK);
  sprite.setCursor(25, 170);
  sprite.println(getWiFiStateString());

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setCursor(10, 198);
  sprite.print("Capteur:");
  sprite.setCursor(25, 218);
  if (ultrasonicState.sensorState) {
    sprite.println("Connecte");
  } else {
    sprite.setTextColor(TFT_RED, TFT_BLACK);
    sprite.println("Erreur");
  }

  sprite.setTextColor(TFT_VIOLET, TFT_BLACK);
  sprite.setCursor(10, 246);
  sprite.print("SD:");
  if (sdOK) {
    sprite.println("OK");
  } else {
    sprite.setTextColor(TFT_RED, TFT_BLACK);
    sprite.println("Error");
  }

  String nowStr = getCurrentDateTime();
  String nowDate = "--/--/----";
  String nowTime = "--:--:--";
  if (nowStr.length() >= 19 && nowStr.charAt(10) == ' ') {
    nowDate = nowStr.substring(0, 10);
    nowTime = nowStr.substring(11, 19);
  }

  // Footer stays small to preserve vertical space for status text above.
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(1);
  sprite.setCursor(10, 275);
  sprite.print("Maintenant:");
  sprite.setCursor(10, 289);
  sprite.print(nowDate + " " + nowTime);

  sprite.pushSprite(0, 0);
}

// Draw information page
// Shows system info: WiFi details, firmware version, etc.
void drawInfoPage() {
  sprite.fillSprite(TFT_BLACK);

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(10, 10);
  sprite.println("Infos");

  drawWiFiArcsIcone(140, 5, wifiQualityRSSI);
  drawWaterLevelSensorStatusIcon(140, 25, ultrasonicState.sensorState);

  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  sprite.setCursor(5, 55);
  sprite.setTextSize(1);
  sprite.print("SSID : ");
  sprite.println(ssid);
  sprite.setCursor(5, 70);
  sprite.print("IP : ");
  sprite.println(Wifi.localIP());
  sprite.setCursor(5, 85);
  sprite.print("Hostname : ");
  sprite.println(String(mdnsName) + ".local");
  sprite.setCursor(5, 100);
  sprite.print("Wifi State : ");
  sprite.println(getWiFiStateString());
  sprite.setCursor(5, 115);
  sprite.print("Signal : ");
  sprite.print(wifiQualityRSSI);
  sprite.println("dBm");

  sprite.setTextColor(TFT_VIOLET, TFT_BLACK);
  sprite.setCursor(5, 135);
  sprite.print("SD : ");
  if (sdOK) {
    uint64_t totalBytes = getSDTotalBytes();
    uint64_t usedBytes = getSDUsedBytes();
    unsigned long usedPercent = totalBytes > 0 ? static_cast<unsigned long>((usedBytes * 100ULL) / totalBytes) : 0;
    sprite.println("OK");
    sprite.setCursor(5, 150);
    sprite.print("Type : ");
    sprite.println(getSDCardTypeName());
    sprite.setCursor(5, 165);
    sprite.print("Used : ");
    sprite.print(usedPercent);
    sprite.println(" %");
    sprite.setCursor(5, 180);
    sprite.print("Used : ");
    sprite.print((unsigned long)(usedBytes / (1024ULL * 1024ULL)));
    sprite.print(" / ");
    sprite.print((unsigned long)(totalBytes / (1024ULL * 1024ULL)));
    sprite.println(" MB");
  } else {
    sprite.setTextColor(TFT_RED, TFT_BLACK);
    sprite.println("Absent");
    sprite.setCursor(5, 150);
    sprite.print("Type : ");
    sprite.println("NONE");
  }

  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(5, 200);
  sprite.println("Firmware");

  sprite.setTextSize(1);
  sprite.setCursor(5, 230);
  sprite.print("Vers. : ");
  sprite.println(VERSION);
  sprite.setCursor(5, 245);
  sprite.print("Auteur : ");
  sprite.println(AUTHOR);
  sprite.setCursor(5, 260);
  sprite.print("Date build : ");
  sprite.println(BUILD_DATE);

  String nowStr = getCurrentDateTime();
  String nowDate = "--/--/----";
  String nowTime = "--:--:--";
  if (nowStr.length() >= 19 && nowStr.charAt(10) == ' ') {
    nowDate = nowStr.substring(0, 10);
    nowTime = nowStr.substring(11, 19);
  }

  sprite.setTextColor(TFT_CYAN, TFT_BLACK);
  sprite.setCursor(5, 280);
  sprite.print("Now: ");
  sprite.println(nowDate);
  sprite.setCursor(5, 295);
  sprite.print("Time: ");
  sprite.println(nowTime);

  sprite.pushSprite(0, 0);
}

// Draw the menu/settings page
// renders the current menu items with selection highlight (in yellow)
void drawMenuPage() {
  sprite.fillScreen(TFT_BLACK);

  if(currentMenu->items[selectedIndex].page != NONE_PAGE_NUM && actived) {
    drawPageNumber(currentMenu->items[selectedIndex].page);
    return;
  }

  // --- Titre ---
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(10, 20);
  sprite.printf(currentMenu->items[0].name);

  // --- Menu ---
  sprite.setTextSize(1);
  int menuLength = currentMenu->size;
  for(int i=1; i<menuLength; i++) {
    uint16_t colorText = (i == selectedIndex && !actived) ? TFT_BLACK : TFT_GREEN;
    uint16_t bgText = (i == selectedIndex && !actived) ? TFT_DARKGREY : TFT_BLACK;
    sprite.setCursor(10, 50 + i*20);
    sprite.setTextColor(colorText, bgText);
    sprite.printf("%s", currentMenu->items[i].name);

    switch(currentMenu->items[i].type){
      case MENU_NONE: break;
      case MENU_INT: {
          sprite.printf(": ");

          uint16_t colorText = (i == selectedIndex) ? TFT_BLACK : TFT_GREEN;
          uint16_t bgText = (i == selectedIndex) ? TFT_DARKGREY : TFT_BLACK;

          sprite.setTextColor(colorText, bgText);
          const int currentValue = *(currentMenu->items[i].intValue);
          sprite.printf("%d", currentValue);
          break;
        }
      case MENU_UNSIGNED_LONG: {
          sprite.printf(": ");

          uint16_t colorText = (i == selectedIndex) ? TFT_BLACK : TFT_GREEN;
          uint16_t bgText = (i == selectedIndex) ? TFT_DARKGREY : TFT_BLACK;

          sprite.setTextColor(colorText, bgText);
          const unsigned long currentValue = *(currentMenu->items[i].unLongValue);
          sprite.printf("%lu", currentValue);
          break;
        }
      case MENU_PRESET_INT:
      case MENU_PRESET_UNSIGNED_LONG: {
          sprite.printf(": ");

          uint16_t colorText = (i == selectedIndex) ? TFT_BLACK : TFT_GREEN;
          uint16_t bgText = (i == selectedIndex) ? TFT_DARKGREY : TFT_BLACK;

          sprite.setTextColor(colorText, bgText);
          const unsigned long currentValue = (currentMenu->items[i].type == MENU_PRESET_INT)
              ? static_cast<unsigned long>(*(currentMenu->items[i].intValue))
              : *(currentMenu->items[i].unLongValue);
          sprite.print(formatPresetLabel(currentValue,
                                         currentMenu->items[i].preset.choices,
                                         currentMenu->items[i].preset.count,
                                         currentMenu->items[i].preset.fallbackUnit));
          break;
        }
      case MENU_BOOL: {
          sprite.printf(": ");

          uint16_t colorText = (i == selectedIndex) ? TFT_BLACK : TFT_GREEN;
          uint16_t bgText = (i == selectedIndex) ? TFT_DARKGREY : TFT_BLACK;

          sprite.setTextColor(colorText, bgText);
          sprite.printf("%s", (*(currentMenu->items[i].boolValue)? "ON": "OFF"));
          break;
        }
    }
  }

  if(actived){
    // fleche indicative des boutons
    sprite.fillTriangle(5, 305, 15, 305, 10, 315, TFT_GREEN);
    sprite.fillTriangle(165, 305, 165, 315, 155, 310, TFT_GREEN);
  }
  else{
    // fleche indicative des boutons
    sprite.fillTriangle(5, 305, 15, 305, 10, 315, TFT_GREEN);
    sprite.fillTriangle(155, 305, 155, 315, 165, 310, TFT_GREEN);
  }

  sprite.pushSprite(0, 0);
}