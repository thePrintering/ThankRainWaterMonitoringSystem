#include "display.h"
#include "settings.h"
#include "constants.h"
#include "wifi_config.h"

extern unsigned long now;

// =================== DISPLAY TIMING VARIABLES ===================
long int lastPressButtonTime = 0;
unsigned long lastPageSwitch = 0;

static DisplayPage getNextDisplayPage(DisplayPage currentPage) {
  switch (currentPage) {
    case GAUGE_PAGE_NUM:
      return GRAPHE_PAGE_NUM;
    case GRAPHE_PAGE_NUM:
      return DATA_PAGE_NUM;
    case DATA_PAGE_NUM:
      return GAUGE_PAGE_NUM;
    default:
      return GAUGE_PAGE_NUM;
  }
}

static DisplayPage getPreviousDisplayPage(DisplayPage currentPage) {
  switch (currentPage) {
    case GAUGE_PAGE_NUM:
      return DATA_PAGE_NUM;
    case GRAPHE_PAGE_NUM:
      return GAUGE_PAGE_NUM;
    case DATA_PAGE_NUM:
      return GRAPHE_PAGE_NUM;
    default:
      return DATA_PAGE_NUM;
  }
}

// Advance to the next page (circular)
void switchPageForward() {
  page = getNextDisplayPage(page);
  lastPageSwitch = now;
}

// Go back to the previous page (circular)
void switchPageBack() {
  page = getPreviousDisplayPage(page);
}

// Route to the correct page-drawing function based on page number
void drawPageNumber(DisplayPage pageNum) {
  switch (pageNum) {
    case GAUGE_PAGE_NUM:
      drawGaugePage();
      break;
    case GRAPHE_PAGE_NUM:
      drawGraphPage();
      break;
    case DATA_PAGE_NUM:
      drawDataPage();
      break;
    case INFO_PAGE_NUM:
      drawInfoPage();
      break;
  }
}

// Draw OTA progress bar during firmware update
// Shows borders and a blue bar scaled to percentage
void drawProgressBarOTA(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint8_t percentage, uint16_t frameColor, uint16_t barColor) {
  if (percentage == 0) {
    tft.fillRoundRect(x0, y0, w, h, 3, TFT_BLACK);
  }
  uint8_t margin = 2;
  uint16_t barHeight = h - 2 * margin;
  uint16_t barWidth = w - 2 * margin;
  tft.drawRoundRect(x0, y0, w, h, 3, frameColor);
  tft.fillRect(x0 + margin, y0 + margin, barWidth * percentage / 100.0, barHeight, barColor);
}

void showRestartConfirmationScreen() {
  // Left/right labels mirror the physical button mapping used by menu.cpp.
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 25);
  tft.println("Restart ESP?");

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 295);
  tft.println("Confirm");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(130, 295);
  tft.println("Cancel");
}

void showRestartScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 30);
  tft.println("Restart...");
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println("ESP rebooting");
}

void showFactoryResetConfirmationScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println("Reset usine?");

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 70);
  tft.println("Efface settings SD");
  tft.setCursor(10, 85);
  tft.println("et credentials NVS.");

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 295);
  tft.println("Confirm");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(130, 295);
  tft.println("Cancel");
}

void showFactoryResetScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 24);
  tft.println("Reset usine");
  tft.setTextSize(1);
  tft.setCursor(10, 58);
  tft.println("Suppression en cours...");
  tft.setCursor(10, 74);
  tft.println("Redemarrage...");
}

void showAPConfirmationScreen() {
  // Confirmation screen for enabling provisioning AP.
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 25);
  tft.println("Enable AP?");

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 70);
  tft.println("Enables setup mode for");
  tft.setCursor(10, 85);
  tft.println("WiFi configuration.");

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 295);
  tft.println("Confirm");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(130, 295);
  tft.println("Cancel");
}

void showAPCredentialsScreen(const String &ssid, const String &password) {
  // Draw static AP credentials screen once.
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_CYAN, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(6, 10);
  sprite.println("Setup AP");

  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(6, 45);
  sprite.println("SSID:");
  sprite.setCursor(6, 65);
  sprite.println(ssid);

  sprite.setCursor(6, 115);
  sprite.println("PASS:");
  sprite.setCursor(6, 135);
  sprite.println(password);

  sprite.setCursor(6, 180);
  sprite.print("IP:");
  sprite.setCursor(6, 200);
  sprite.println(WiFi.softAPIP());

  updateAPConnectionStatus(0);
  updateAPCountdown(0);
  updateAPWiFiStatus();

  sprite.setTextSize(1);
  sprite.setTextColor(TFT_MAGENTA, TFT_BLACK);
  sprite.setCursor(10, 300);
  sprite.println("Press button to exit");

  sprite.pushSprite(0, 0);
}

void updateAPConnectionStatus(int connectedClients) {
  // Update only the status row to avoid screen flicker.
  sprite.fillRect(6, 248, 300, 16, TFT_BLACK);
  sprite.setTextSize(1);
  sprite.setCursor(6, 248);
  if (connectedClients > 0) {
    sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    sprite.print("Client connected: ");
    sprite.print(connectedClients);
  } else {
    sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
    sprite.print("No client connected");
  }

  sprite.pushSprite(0, 0);
}

void updateAPCountdown(unsigned long remainingSeconds) {
  // Update only the countdown row to avoid screen flicker.
  sprite.fillRect(6, 232, 300, 16, TFT_BLACK);
  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  sprite.setTextSize(1);
  sprite.setCursor(6, 232);
  sprite.print("Auto-off in: ");
  sprite.print(remainingSeconds);
  sprite.print(" sec");

  sprite.pushSprite(0, 0);
}

void updateAPWiFiStatus() {
  // Show STA connection attempt/progress while AP is active.
  sprite.fillRect(6, 264, 300, 32, TFT_BLACK);
  sprite.setTextSize(1);
  sprite.setCursor(6, 264);
  sprite.setTextColor(getWiFiColor(), TFT_BLACK);
  sprite.print("WiFi: ");
  sprite.print(getWiFiStateString());

  if (wifiState == WIFI_CONNECTED) {
    sprite.print(" ");
    sprite.print(getCurrentWiFiSSID());
  }

  const String hint = getWiFiProvisioningHint();
  if (hint.length() > 0) {
    sprite.setCursor(6, 280);
    sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
    sprite.print(hint);
  }

  sprite.pushSprite(0, 0);
}

// Main display update function called from loop()
// Handles page auto-advancing, mode switching, and brightness control
void updateTFT(unsigned long now) {
  // Auto-advance page if time elapsed since last switch exceeds page interval
  if ((mode == STANDBY_MODE_NUM || mode == AUTO_MODE_NUM) && now - lastPageSwitch > settings.display.pageInterval.value * 1000) {
    lastPageSwitch = now;
    switchPageForward();
  }

  // Draw the current page (or menu if in settings mode)
  switch (mode) {
    case STANDBY_MODE_NUM:
    case AUTO_MODE_NUM:
    case MANU_MODE_NUM:
      // Normal operation: draw the selected page
      drawPageNumber(page);
      break;
    case SETTING_MODE_NUM:
      // Settings mode: draw menu
      drawMenuPage();
      break;
  }

  // Inactivity handling:
  // 1) MANU -> AUTO after timeout
  // 2) AUTO -> STANDBY after timeout only if standby is enabled
  if (now - lastPressButtonTime > settings.display.standbyInterval.value * 1000) {
    if (mode == MANU_MODE_NUM) {
      mode = AUTO_MODE_NUM;
      // Restart inactivity timer so standby does not trigger immediately.
      lastPressButtonTime = now;
    } else if (settings.display.standbyActived && mode == AUTO_MODE_NUM) {
      mode = STANDBY_MODE_NUM;
    }
  }

  // Adjust brightness: standby mode uses low brightness, others use active setting.
  const int selectedBrightnessLevel = (mode == STANDBY_MODE_NUM)
      ? settings.display.standbyBright.value
      : settings.display.bright.value;
  ledcWrite(0, getDisplayBrightnessPwm(selectedBrightnessLevel));
}