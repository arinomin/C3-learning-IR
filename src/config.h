// File: src/config.h
#pragma once
#include <Arduino.h>

// ピンアサイン
constexpr uint8_t PIN_I2C_SDA      = 0;
constexpr uint8_t PIN_I2C_SCL      = 1;
constexpr uint8_t PIN_STATUS_LED   = 2;
constexpr uint8_t PIN_BTN_SWITCH   = 3;
constexpr uint8_t PIN_IR_SEND      = 4;
constexpr uint8_t PIN_IR_RECV      = 5;
constexpr uint8_t PIN_BTN_SEND     = 6;

// スロット数（拡張時はここを変更）
constexpr uint8_t SLOT_COUNT       = 5;

// タイミング
constexpr uint32_t DEBOUNCE_MS         = 30;
constexpr uint32_t LONG_PRESS_MS       = 1000;
constexpr uint32_t LEARN_TIMEOUT_MS    = 10000;
constexpr uint32_t MENU_TIMEOUT_MS     = 30000;
constexpr uint32_t SENT_FLASH_MS       = 600;
constexpr uint32_t LEARN_BLINK_MS      = 250;

// OLED
constexpr uint8_t  OLED_WIDTH      = 128;
constexpr uint8_t  OLED_HEIGHT     = 32;
constexpr uint8_t  OLED_ADDR       = 0x3C;