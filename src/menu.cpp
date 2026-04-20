#include "menu.h"
#include "settings.h"
#include "display.h"
#include "constants.h"
#include "storage.h"
#include "wifi_config.h"
#include "wifi_provisioning.h"

// Global menu cursor/context shared with display rendering code.
Menu* currentMenu = nullptr;
int selectedIndex = 1;
bool actived = false;

extern Menu displayMenu;
extern Menu mainMenu;
extern Menu wifiMenu;
extern Menu standbyMenu;

static MenuItem makeIntMenuItem(const char* name,
                                int* value,
                                int min,
                                int max,
                                int step,
                                Menu* nextMenu,
                                DisplayPage page);

static MenuItem makeULongMenuItem(const char* name,
                                  unsigned long* value,
                                  unsigned long min,
                                  unsigned long max,
                                  unsigned long step,
                                  Menu* nextMenu,
                                  DisplayPage page);

static MenuItem makePresetIntMenuItem(const char* name,
                                      int* value,
                                      const PresetChoice* presets,
                                      size_t presetCount,
                                      const char* fallbackUnit,
                                      Menu* nextMenu,
                                      DisplayPage page);

static MenuItem makePresetULongMenuItem(const char* name,
                                        unsigned long* value,
                                        const PresetChoice* presets,
                                        size_t presetCount,
                                        const char* fallbackUnit,
                                        Menu* nextMenu,
                                        DisplayPage page);

static MenuItem makeNoneMenuItem(const char* name,
                                 Menu* nextMenu,
                                 DisplayPage page);

static MenuItem makeBoolMenuItem(const char* name,
                                 bool* value,
                                 Menu* nextMenu,
                                 DisplayPage page);

static MenuItem makeIntMenuItem(const char* name,
                                int* value,
                                int min,
                                int max,
                                int step,
                                Menu* nextMenu,
                                DisplayPage page) {
  MenuItem item;
  item.name = name;
  item.type = MENU_INT;
  item.intValue = value;
  item.min = static_cast<unsigned long>(min);
  item.max = static_cast<unsigned long>(max);
  item.step = static_cast<unsigned long>(step);
  item.preset = {nullptr, 0, nullptr};
  item.nextMenu = nextMenu;
  item.page = page;
  return item;
}

static MenuItem makeULongMenuItem(const char* name,
                                  unsigned long* value,
                                  unsigned long min,
                                  unsigned long max,
                                  unsigned long step,
                                  Menu* nextMenu,
                                  DisplayPage page) {
  MenuItem item;
  item.name = name;
  item.type = MENU_UNSIGNED_LONG;
  item.unLongValue = value;
  item.min = static_cast<long long>(min);
  item.max = static_cast<long long>(max);
  item.step = static_cast<long long>(step);
  item.preset = {nullptr, 0, nullptr};
  item.nextMenu = nextMenu;
  item.page = page;
  return item;
}

static MenuItem makePresetIntMenuItem(const char* name,
                                      int* value,
                                      const PresetChoice* presets,
                                      size_t presetCount,
                                      const char* fallbackUnit,
                                      Menu* nextMenu,
                                      DisplayPage page) {
  const unsigned long normalized = normalizePresetValue(static_cast<unsigned long>(*value), presets, presetCount);
  *value = static_cast<int>(normalized);
  MenuItem item;
  item.name = name;
  item.type = MENU_PRESET_INT;
  item.intValue = value;
  item.min = 0;
  item.max = 0;
  item.step = 0;
  item.preset = {presets, presetCount, fallbackUnit};
  item.nextMenu = nextMenu;
  item.page = page;
  return item;
}

static MenuItem makePresetULongMenuItem(const char* name,
                                        unsigned long* value,
                                        const PresetChoice* presets,
                                        size_t presetCount,
                                        const char* fallbackUnit,
                                        Menu* nextMenu,
                                        DisplayPage page) {
  *value = normalizePresetValue(*value, presets, presetCount);
  MenuItem item;
  item.name = name;
  item.type = MENU_PRESET_UNSIGNED_LONG;
  item.unLongValue = value;
  item.min = 0;
  item.max = 0;
  item.step = 0;
  item.preset = {presets, presetCount, fallbackUnit};
  item.nextMenu = nextMenu;
  item.page = page;
  return item;
}

static MenuItem makeNoneMenuItem(const char* name,
                                 Menu* nextMenu,
                                 DisplayPage page) {
  MenuItem item;
  item.name = name;
  item.type = MENU_NONE;
  item.intValue = nullptr;
  item.min = 0;
  item.max = 0;
  item.step = 0;
  item.preset = {nullptr, 0, nullptr};
  item.nextMenu = nextMenu;
  item.page = page;
  return item;
}

static MenuItem makeBoolMenuItem(const char* name,
                                 bool* value,
                                 Menu* nextMenu,
                                 DisplayPage page) {
  MenuItem item;
  item.name = name;
  item.type = MENU_BOOL;
  item.boolValue = value;
  item.min = 0;
  item.max = 1;
  item.step = 1;
  item.preset = {nullptr, 0, nullptr};
  item.nextMenu = nextMenu;
  item.page = page;
  return item;
}

static void cyclePresetValue(unsigned long &value, const PresetChoice* presets, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (presets[i].value == value) {
      value = presets[(i + 1) % count].value;
      return;
    }
  }

  value = presets[0].value;
}

static void cycleCurrentPresetIntValue() {
  MenuItem &item = currentMenu->items[selectedIndex];
  if (!item.preset.choices || item.preset.count == 0) {
    return;
  }

  unsigned long currentValue = static_cast<unsigned long>(*item.intValue);
  currentValue = normalizePresetValue(currentValue, item.preset.choices, item.preset.count);
  cyclePresetValue(currentValue, item.preset.choices, item.preset.count);
  *item.intValue = static_cast<int>(currentValue);
}

static void cycleCurrentPresetULongValue() {
  MenuItem &item = currentMenu->items[selectedIndex];
  if (!item.preset.choices || item.preset.count == 0) {
    return;
  }

  unsigned long currentValue = *item.unLongValue;
  currentValue = normalizePresetValue(currentValue, item.preset.choices, item.preset.count);
  cyclePresetValue(currentValue, item.preset.choices, item.preset.count);
  *item.unLongValue = currentValue;
}

static void cycleCurrentULongValue() {
  unsigned long* value = currentMenu->items[selectedIndex].unLongValue;
  const unsigned long minValue = static_cast<unsigned long>(currentMenu->items[selectedIndex].min);
  const unsigned long maxValue = static_cast<unsigned long>(currentMenu->items[selectedIndex].max);
  const unsigned long stepValue = static_cast<unsigned long>(currentMenu->items[selectedIndex].step);

  *value += stepValue;
  if (*value > maxValue) {
    *value = minValue;
  }
}

MenuItem mainItems[] = {
  makeNoneMenuItem("Parametres", nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("Informations  ->", nullptr, INFO_PAGE_NUM),
  makeNoneMenuItem("Display  ->", &displayMenu, NONE_PAGE_NUM),
  makeNoneMenuItem("Mesure  ->", &measureMenu, NONE_PAGE_NUM),
  makeNoneMenuItem("Capteur  ->", &capteurMenu, NONE_PAGE_NUM),
  makeNoneMenuItem("WiFi  ->", &wifiMenu, NONE_PAGE_NUM),
  makeNoneMenuItem("Reset usine", nullptr, FACTORY_RESET_ACTION_PAGE),
  makeNoneMenuItem("Redemarrer ESP", nullptr, RESTART_ACTION_PAGE)
};

MenuItem wifiItems[] = {
  makeNoneMenuItem("WiFi", nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("<-'", &mainMenu, NONE_PAGE_NUM),
  makeBoolMenuItem("WiFi active", &settings.wifi.enabled, nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("AP Manuel", nullptr, AP_ACTION_PAGE)
};

MenuItem displayItems[] = {
  makeNoneMenuItem("Display", nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("<-'", &mainMenu, NONE_PAGE_NUM),
  makeNoneMenuItem("Mode Repos  ->", &standbyMenu, NONE_PAGE_NUM),
  makePresetIntMenuItem("Brightness", &settings.display.bright.value, DISPLAY_BRIGHTNESS_PRESETS, DISPLAY_BRIGHTNESS_PRESET_COUNT, "lvl", nullptr, NONE_PAGE_NUM),
  makePresetIntMenuItem("Page Interval", &settings.display.pageInterval.value, PAGE_INTERVAL_PRESETS, PAGE_INTERVAL_PRESET_COUNT, "sec", nullptr, NONE_PAGE_NUM)
};

MenuItem standbyItems[] = {
  makeNoneMenuItem("Standby", nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("<-'", &displayMenu, NONE_PAGE_NUM),
  makeBoolMenuItem("Activee", &settings.display.standbyActived, nullptr, NONE_PAGE_NUM),
  makePresetIntMenuItem("Standby Brightness", &settings.display.standbyBright.value, DISPLAY_BRIGHTNESS_PRESETS, DISPLAY_BRIGHTNESS_PRESET_COUNT, "lvl", nullptr, NONE_PAGE_NUM),
  makeIntMenuItem("Standby Interval", &settings.display.standbyInterval.value, settings.display.standbyInterval.min, settings.display.standbyInterval.max, settings.display.standbyInterval.step, nullptr, NONE_PAGE_NUM)
};

MenuItem measureItems[] = {
  makeNoneMenuItem("Mesure", nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("<-'", &mainMenu, NONE_PAGE_NUM),
  makePresetULongMenuItem("Periode mesure", &settings.mesure.measurePeriod.value, MEASURE_PERIOD_PRESETS, MEASURE_PERIOD_PRESET_COUNT, "ms", nullptr, NONE_PAGE_NUM),
  makePresetULongMenuItem("Sample Interval", &settings.mesure.sampleInterval.value, SENSOR_SAMPLE_INTERVAL_PRESETS, SENSOR_SAMPLE_INTERVAL_PRESET_COUNT, "ms", nullptr, NONE_PAGE_NUM),
  makeULongMenuItem("Sample Count", &settings.mesure.sampleCount.value, settings.mesure.sampleCount.min, settings.mesure.sampleCount.max, settings.mesure.sampleCount.step, nullptr, NONE_PAGE_NUM),
  makePresetULongMenuItem("Calcule samples", &settings.mesure.sampleMethod.value, SENSOR_SAMPLE_METHOD_PRESETS, SENSOR_SAMPLE_METHOD_PRESET_COUNT, "", nullptr, NONE_PAGE_NUM),
};

MenuItem capteurItems[] = {
  makeNoneMenuItem("Capteur", nullptr, NONE_PAGE_NUM),
  makeNoneMenuItem("<-'", &mainMenu, NONE_PAGE_NUM),
  makeBoolMenuItem("Spike Detection", &settings.sensor.enableSpikeDetection, nullptr, NONE_PAGE_NUM),
  makeULongMenuItem("Seuil Spike", &settings.sensor.spikeThreshold_mm.value, settings.sensor.spikeThreshold_mm.min, settings.sensor.spikeThreshold_mm.max, settings.sensor.spikeThreshold_mm.step, nullptr, NONE_PAGE_NUM),
  makeBoolMenuItem("Filtre IQR", &settings.sensor.enableIqrFilter, nullptr, NONE_PAGE_NUM),
};

Menu capteurMenu = {
  capteurItems,
  sizeof(capteurItems) / sizeof(capteurItems[0])
};

Menu displayMenu = {
  displayItems,
  sizeof(displayItems) / sizeof(displayItems[0])
};

Menu mainMenu = {
  mainItems,
  sizeof(mainItems) / sizeof(mainItems[0])
};

Menu wifiMenu = {
  wifiItems,
  sizeof(wifiItems) / sizeof(wifiItems[0])
};

Menu standbyMenu = {
  standbyItems,
  sizeof(standbyItems) / sizeof(standbyItems[0])
};

Menu measureMenu = {
  measureItems,
  sizeof(measureItems) / sizeof(measureItems[0])
};

static bool processMenuButton(Button &btn,
                              unsigned long now,
                              void (*onShortPress)(),
                              void (*onLongPress)()) {
  return inputProcessButton(btn,
                            now,
                            settings.button.debounceTime.value,
                            settings.button.longPressTime.value,
                            onShortPress,
                            onLongPress);
}

static void cycleCurrentIntValue() {
  int *value = currentMenu->items[selectedIndex].intValue;

  *value += static_cast<int>(currentMenu->items[selectedIndex].step);
  if (*value > static_cast<int>(currentMenu->items[selectedIndex].max)) {
    *value = static_cast<int>(currentMenu->items[selectedIndex].min);
  }
}

static void toggleCurrentBoolValue() {
  bool *value = currentMenu->items[selectedIndex].boolValue;
  *value = !(*value);
  saveSettings();
}

static void applyPostShortPressModeTransition(DisplayMode modeBeforeInput, DisplayMode &mode) {
  // Any user short press wakes standby and unlocks manual mode from AUTO.
  if (modeBeforeInput == STANDBY_MODE_NUM) {
    mode = AUTO_MODE_NUM;
  } else if (modeBeforeInput == AUTO_MODE_NUM) {
    mode = MANU_MODE_NUM;
  }
}

static bool confirmRestartAction() {
  // Confirmation is intentionally modal to prevent accidental reboot from menu navigation.
  showRestartConfirmationScreen();
  waitForUserButtonsRelease();

  while (true) {
    waitForAnyUserButtonPress();

    // BTN1 confirms restart (left/confirm label on TFT screen).
    if (isButtonPressed(btn1)) {
      waitForUserButtonsRelease();
      return true;
    }

    // BTN2 cancels and returns to menu context.
    if (isButtonPressed(btn2)) {
      waitForUserButtonsRelease();
      return false;
    }

    waitForUserButtonsRelease();
  }
}

static bool confirmAPAction() {
  // Confirmation is intentionally modal to prevent accidental AP activation from menu navigation.
  extern unsigned long now;
  showAPConfirmationScreen();
  waitForUserButtonsRelease();

  while (true) {
    waitForAnyUserButtonPress();

    // BTN1 confirms AP activation (left/confirm label on TFT screen).
    if (isButtonPressed(btn1)) {
      waitForUserButtonsRelease();
      return true;
    }

    // BTN2 cancels and returns to menu context.
    if (isButtonPressed(btn2)) {
      waitForUserButtonsRelease();
      return false;
    }

    waitForUserButtonsRelease();
  }
}

static bool confirmFactoryResetAction() {
  // Confirmation is intentionally modal to prevent accidental data wipe.
  showFactoryResetConfirmationScreen();
  waitForUserButtonsRelease();

  while (true) {
    waitForAnyUserButtonPress();

    // BTN1 confirms factory reset.
    if (isButtonPressed(btn1)) {
      waitForUserButtonsRelease();
      return true;
    }

    // BTN2 cancels and returns to menu context.
    if (isButtonPressed(btn2)) {
      waitForUserButtonsRelease();
      return false;
    }

    waitForUserButtonsRelease();
  }
}

static void showAPCredentialsAndWaitForExit() {
  // Display AP credentials with live client status and wait for user to press button to exit.
  unsigned long lastStatusDrawMs = 0;
  unsigned long lastCountdownDrawMs = 0;
  unsigned long lastWiFiStateDrawMs = 0;

  waitForUserButtonsRelease();

  const String ssid = getProvisioningAPSsid();
  const String password = getProvisioningAPPassword();
  showAPCredentialsScreen(ssid, password);

  while (isWiFiProvisioningAPActive()) {
    const unsigned long loopNow = millis();
    // The main loop is paused while this modal screen is open, so run WiFi updates here.
    extern unsigned long now;
    now = loopNow;
    updateWifi();

    checkProvisioningAPTimeout(loopNow);

    if (!isWiFiProvisioningAPActive()) {
      break;
    }

    // Refresh only the connection status row.
    if (loopNow - lastStatusDrawMs >= 300) {
      updateAPConnectionStatus(getProvisioningAPClientCount());
      lastStatusDrawMs = loopNow;
    }

    // Refresh only the countdown row once per second.
    if (loopNow - lastCountdownDrawMs >= 1000) {
      updateAPCountdown(getProvisioningAPRemainingTime(loopNow));
      lastCountdownDrawMs = loopNow;
    }

    // Refresh WiFi station connection state while AP stays visible.
    if (loopNow - lastWiFiStateDrawMs >= 500) {
      updateAPWiFiStatus();
      lastWiFiStateDrawMs = loopNow;
    }

    // Check for button press to exit early
    if (isButtonPressed(btn1) || isButtonPressed(btn2)) {
      waitForUserButtonsRelease();
      break;
    }

    delay(20);
  }
}

void onEnterSettingsLongPress() {
  extern DisplayMode mode;
  mode = SETTING_MODE_NUM;
}

void onMenuPrimaryShortPress() {
  if (actived) {
    if (currentMenu->items[selectedIndex].page != NONE_PAGE_NUM) {
      actived = false;
    } else {
      switch (currentMenu->items[selectedIndex].type) {
        case MENU_INT:
          // Value edition cycles in [min..max] with configured step.
          cycleCurrentIntValue();
          break;
        case MENU_UNSIGNED_LONG:
          cycleCurrentULongValue();
          break;
        case MENU_PRESET_INT:
          cycleCurrentPresetIntValue();
          break;
        case MENU_PRESET_UNSIGNED_LONG:
          cycleCurrentPresetULongValue();
          break;
        case MENU_BOOL:
          toggleCurrentBoolValue();
          break;
        default:
          break;
      }
    }
  } else {
    // Navigation mode: move cursor to next editable/enterable item.
    selectedIndex = (selectedIndex + 1) % currentMenu->size;
    if (selectedIndex == 0) {
      selectedIndex = 1;
    }
  }
}

void onMenuSecondaryShortPress() {
  MenuItem &selectedItem = currentMenu->items[selectedIndex];
  extern unsigned long now;
  extern bool networkServicesStarted;
  extern void startNetworkServices();

  // Dedicated menu action: restart with explicit user confirmation.
  if (selectedItem.page == RESTART_ACTION_PAGE) {
    if (confirmRestartAction()) {
      showRestartScreen();
      delay(1500); // Allow user to see restart screen before rebooting.
      ESP.restart();
    }
    return;
  }

  // Dedicated menu action: factory reset (SD settings + NVS WiFi credentials).
  if (selectedItem.page == FACTORY_RESET_ACTION_PAGE) {
    if (confirmFactoryResetAction()) {
      showFactoryResetScreen();

      if (ensureSDReady() && sdExists("/settings.json")) {
        removeSDFile("/settings.json");
      }

      clearProvisionedWiFiCredentials();

      delay(1500);
      ESP.restart();
    }
    return;
  }

  // Dedicated menu action: manual AP provisioning with confirmation
  if (selectedItem.page == AP_ACTION_PAGE) {
    if (confirmAPAction()) {
      startProvisioningAP(now, networkServicesStarted, startNetworkServices);
      showAPCredentialsAndWaitForExit();
      stopProvisioningAP();
    }
    return;
  }

  if (actived) {
    actived = false;
  } else {
    if (selectedItem.page != NONE_PAGE_NUM ||
        selectedItem.type != MENU_NONE) {
      actived = true;
    } else {
      if (selectedItem.nextMenu) {
        // Enter submenu.
        currentMenu = selectedItem.nextMenu;
        selectedIndex = 1;
        actived = false;
      }
    }
  }
}

void onExitSettingsLongPress() {
  extern DisplayMode mode;
  extern void saveSettings();

  // Long press exits settings and persists current values.
  mode = MANU_MODE_NUM;
  currentMenu = &mainMenu;
  selectedIndex = 1;
  actived = false;
  saveSettings();
}

void updateAllButtonState() {
  extern DisplayMode mode;
  extern unsigned long now;

  const DisplayMode modeBeforeInput = mode;
  bool shortPressDetected = false;

  // Input events are produced by input module, menu only maps events to actions.
  switch (mode) {
    case STANDBY_MODE_NUM:
      shortPressDetected |= processMenuButton(btn1, now, nullptr, nullptr);
      shortPressDetected |= processMenuButton(btn2, now, nullptr, nullptr);
      break;
    case AUTO_MODE_NUM:
    case MANU_MODE_NUM:
      shortPressDetected |= processMenuButton(btn1, now, switchPageBack, nullptr);
      shortPressDetected |= processMenuButton(btn2, now, switchPageForward, onEnterSettingsLongPress);
      break;
    case SETTING_MODE_NUM:
      shortPressDetected |= processMenuButton(btn1, now, onMenuPrimaryShortPress, nullptr);
      shortPressDetected |= processMenuButton(btn2, now, onMenuSecondaryShortPress, onExitSettingsLongPress);
      break;
    default:
      break;
  }

  if (shortPressDetected) {
    applyPostShortPressModeTransition(modeBeforeInput, mode);
  }

  lastPressButtonTime = max(btn1.pressTime, btn2.pressTime);
}
