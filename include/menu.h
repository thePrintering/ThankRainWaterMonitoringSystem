#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "constants.h"
#include "input.h"

// =================== MENU ENUMS ===================
enum MenuValueType {
  MENU_NONE,
  MENU_INT,
  MENU_UNSIGNED_LONG,
  MENU_BOOL,
  MENU_PRESET_INT,
  MENU_PRESET_UNSIGNED_LONG
};

// =================== MENU STRUCTURES ===================
struct Menu;

struct MenuPresetMeta {
  const PresetChoice* choices;
  size_t count;
  const char* fallbackUnit;
};

struct MenuItem {
  const char* name;
  MenuValueType type;
  union {
    int* intValue;
    bool* boolValue;
    unsigned long* unLongValue;
  };
  
  long long min, max, step;
  MenuPresetMeta preset;
  Menu* nextMenu;
  DisplayPage page;
};

struct Menu {
  MenuItem* items;
  uint8_t size;
};

// =================== GLOBAL VARIABLES ===================
extern Menu* currentMenu;
extern int selectedIndex;
extern bool actived;

extern Menu displayMenu;
extern Menu mainMenu;
extern Menu wifiMenu;
extern Menu standbyMenu;
extern Menu measureMenu;
extern Menu capteurMenu;

// =================== FUNCTION DECLARATIONS ===================
void updateAllButtonState();

// Button handlers
void onEnterSettingsLongPress();
void onMenuPrimaryShortPress();
void onMenuSecondaryShortPress();
void onExitSettingsLongPress();

#endif // MENU_H
