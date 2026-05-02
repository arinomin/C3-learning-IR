// File: src/slot_store.h
#pragma once
#include <Arduino.h>
#include <IRremoteESP8266.h>

struct SlotData {
  bool        used;
  decode_type_t protocol;
  uint64_t    value;
  uint16_t    bits;
  String      name;       // プロトコル名（OLED表示用）
};

namespace SlotStore {
  void   begin();
  void   loadAll();
  bool   save(uint8_t idx, const SlotData& data);
  bool   clear(uint8_t idx);
  const  SlotData& get(uint8_t idx);
  void   setCurrentSlot(uint8_t idx);
  uint8_t getCurrentSlot();
}