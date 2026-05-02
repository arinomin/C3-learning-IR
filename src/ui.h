// File: src/ui.h
#pragma once
#include <Arduino.h>
#include "slot_store.h"

namespace UI {
  void begin();
  void showSendMode(uint8_t slot, const SlotData& data, bool justSent);
  void showLearnSelect(uint8_t slot, const SlotData& data);
  void showLearnMenu(uint8_t slot, const SlotData& data, uint8_t menuIdx);
  void showLearnWait(uint8_t slot, uint32_t remainSec);
  void showLearnSaveConfirm(const SlotData& captured);
  void showLearnDeleteConfirm(uint8_t slot, const SlotData& data);
  void showMessage(const char* line1, const char* line2);
}