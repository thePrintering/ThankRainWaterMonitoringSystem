#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

// Low-level button state shared across modules (menu, OTA, future features).
struct Button {
  uint8_t pin;
  bool lastState;
  unsigned long pressTime;
  bool longTriggered;
  const bool pressState;
};

extern Button btn1;
extern Button btn2;

void inputInit();
bool isButtonPressed(const Button &btn);
bool isAnyUserButtonPressed();
void waitForUserButtonsRelease();
void waitForAnyUserButtonPress();

// Generic button event processor with configurable timings.
bool inputProcessButton(Button &btn,
                        unsigned long nowMs,
                        unsigned long debounceMs,
                        unsigned long longPressMs,
                        void (*onShortPress)(),
                        void (*onLongPress)());

#endif // INPUT_H