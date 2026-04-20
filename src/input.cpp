#include "input.h"
#include "constants.h"

Button btn1 = {B1_PIN, HIGH, 0, false, LOW};
Button btn2 = {B2_PIN, HIGH, 0, false, LOW};

void inputInit() {
  pinMode(btn1.pin, INPUT_PULLUP);
  pinMode(btn2.pin, INPUT_PULLUP);
}

bool isButtonPressed(const Button &btn) {
  return digitalRead(btn.pin) == btn.pressState;
}

bool isAnyUserButtonPressed() {
  return isButtonPressed(btn1) || isButtonPressed(btn2);
}

void waitForUserButtonsRelease() {
  while (isAnyUserButtonPressed()) {
    delay(20);
    yield();
  }
}

void waitForAnyUserButtonPress() {
  while (!isAnyUserButtonPressed()) {
    delay(20);
    yield();
  }
}

bool inputProcessButton(Button &btn,
                        unsigned long nowMs,
                        unsigned long debounceMs,
                        unsigned long longPressMs,
                        void (*onShortPress)(),
                        void (*onLongPress)()) {
  bool shortTriggered = false;
  bool state = digitalRead(btn.pin);

  if (state != btn.lastState) {
    // Keep same debounce behavior as before, but centralized for all modules.
    delay(debounceMs);
    state = digitalRead(btn.pin);
  }

  if (state == btn.pressState && btn.lastState != btn.pressState) {
    btn.pressTime = nowMs;
    btn.longTriggered = false;
  }

  if (state == btn.pressState && !btn.longTriggered) {
    if (nowMs - btn.pressTime > longPressMs) {
      btn.longTriggered = true;
      if (onLongPress) onLongPress();
    }
  }

  if (state != btn.pressState && btn.lastState == btn.pressState) {
    if (!btn.longTriggered) {
      shortTriggered = true;
      if (onShortPress) {
        onShortPress();
      }
    }
  }

  btn.lastState = state;
  return shortTriggered;
}