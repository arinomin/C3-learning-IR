// File: src/buttons.h
#pragma once
#include <Arduino.h>
#include "config.h"

enum class ButtonEvent : uint8_t {
  None,
  ShortPress,
  LongPress
};

class Button {
public:
  Button(uint8_t pin) : pin_(pin) {}

  void begin() {
    pinMode(pin_, INPUT_PULLUP);
    lastReading_ = digitalRead(pin_);
    stableState_ = lastReading_;
    lastChange_ = millis();
  }

  // 毎ループ呼び出す。確定したイベントを返す。
  ButtonEvent update() {
    uint32_t now = millis();
    bool reading = digitalRead(pin_);

    if (reading != lastReading_) {
      lastChange_ = now;
      lastReading_ = reading;
    }

    if ((now - lastChange_) >= DEBOUNCE_MS && reading != stableState_) {
      stableState_ = reading;
      if (stableState_ == LOW) {
        // 押下開始
        pressStart_ = now;
        longFired_ = false;
      } else {
        // 離した
        if (!longFired_) {
          return ButtonEvent::ShortPress;
        }
      }
    }

    // 押されている間に長押し閾値を超えたら即イベント
    if (stableState_ == LOW && !longFired_ &&
        (now - pressStart_) >= LONG_PRESS_MS) {
      longFired_ = true;
      return ButtonEvent::LongPress;
    }

    return ButtonEvent::None;
  }

  bool isPressed() const { return stableState_ == LOW; }

private:
  uint8_t  pin_;
  bool     lastReading_ = HIGH;
  bool     stableState_ = HIGH;
  uint32_t lastChange_ = 0;
  uint32_t pressStart_ = 0;
  bool     longFired_  = false;
};