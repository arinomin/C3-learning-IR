// File: src/ui.cpp
#include "ui.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace {
  Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

  void drawHeader(const char* left, const char* right) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(left);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(right, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(OLED_WIDTH - w, 0);
    display.print(right);
  }

  void drawBody(const String& text) {
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.print(text);
  }

  String formatSignal(const SlotData& s) {
    if (!s.used) return "(empty)";
    String r = s.name.length() ? s.name : "UNKNOWN";
    r += "\n0x"; // ★ " 0x" から "\n0x" に変更（\nが改行を意味します）
    char buf[20];
    snprintf(buf, sizeof(buf), "%llX", (unsigned long long)s.value);
    r += buf;
    return r;
  }
}

namespace UI {

void begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void showSendMode(uint8_t slot, const SlotData& data, bool justSent) {
  display.clearDisplay();
  char left[32];
  snprintf(left, sizeof(left), "Slot %u/%u", slot + 1, SLOT_COUNT);
  drawHeader(left, justSent ? "[SENT!]" : "[SEND]");
  drawBody(formatSignal(data));
  display.display();
}

void showLearnSelect(uint8_t slot, const SlotData& data) {
  display.clearDisplay();
  char left[32];
  snprintf(left, sizeof(left), "LEARN Slot %u/%u", slot + 1, SLOT_COUNT);
  drawHeader(left, "[SELECT]");
  drawBody(formatSignal(data));
  display.display();
}

void showLearnMenu(uint8_t slot, const SlotData& data, uint8_t menuIdx) {
  display.clearDisplay();
  char left[32];
  snprintf(left, sizeof(left), "LEARN Slot %u/%u", slot + 1, SLOT_COUNT);
  drawHeader(left, "[MENU]");
  String body;
  body  = (menuIdx == 0) ? ">Overwrite" : " Overwrite";
  body += (menuIdx == 1) ? " >Delete"   : "  Delete";
  drawBody(body);
  display.display();
}

void showLearnWait(uint8_t slot, uint32_t remainSec) {
  display.clearDisplay();
  char left[32];
  snprintf(left, sizeof(left), "LEARN Slot %u/%u", slot + 1, SLOT_COUNT);
  char right[16];
  snprintf(right, sizeof(right), "[WAIT %lus]", (unsigned long)remainSec);
  drawHeader(left, right);
  drawBody("Point remote & press");
  display.display();
}

void showLearnSaveConfirm(const SlotData& captured) {
  display.clearDisplay();
  drawHeader("Captured!", "[SAVE?]");
  drawBody(formatSignal(captured));
  display.display();
}

void showLearnDeleteConfirm(uint8_t slot, const SlotData& data) {
  display.clearDisplay();
  char left[32];
  snprintf(left, sizeof(left), "Delete Slot %u?", slot + 1);
  drawHeader(left, "[Y=SEND]");
  drawBody(formatSignal(data));
  display.display();
}

void showMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  drawHeader(line1, "");
  drawBody(line2);
  display.display();
}

} // namespace