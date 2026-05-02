// File: src/slot_store.cpp
#include "slot_store.h"
#include "config.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  SlotData slots[SLOT_COUNT];
  uint8_t currentSlot = 0;
  const char* NS = "irslot";

  String key(const char* base, uint8_t idx) {
    String s = base;
    s += idx;
    return s;
  }
}

namespace SlotStore {

void begin() {
  prefs.begin(NS, false);
}

void loadAll() {
  currentSlot = prefs.getUChar("cur", 0);
  if (currentSlot >= SLOT_COUNT) currentSlot = 0;

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    SlotData& s = slots[i];
    s.used     = prefs.getBool(key("u", i).c_str(), false);
    s.protocol = (decode_type_t)prefs.getInt(key("p", i).c_str(), (int)decode_type_t::UNKNOWN);
    s.value    = prefs.getULong64(key("v", i).c_str(), 0);
    s.bits     = prefs.getUShort(key("b", i).c_str(), 0);
    s.name     = prefs.getString(key("n", i).c_str(), "");
  }
}

bool save(uint8_t idx, const SlotData& data) {
  if (idx >= SLOT_COUNT) return false;
  slots[idx] = data;
  prefs.putBool(key("u", idx).c_str(), data.used);
  prefs.putInt(key("p", idx).c_str(), (int)data.protocol);
  prefs.putULong64(key("v", idx).c_str(), data.value);
  prefs.putUShort(key("b", idx).c_str(), data.bits);
  prefs.putString(key("n", idx).c_str(), data.name);
  return true;
}

bool clear(uint8_t idx) {
  if (idx >= SLOT_COUNT) return false;
  SlotData empty = {};
  empty.used = false;
  empty.protocol = decode_type_t::UNKNOWN;
  empty.value = 0;
  empty.bits = 0;
  empty.name = "";
  return save(idx, empty);
}

const SlotData& get(uint8_t idx) {
  return slots[idx < SLOT_COUNT ? idx : 0];
}

void setCurrentSlot(uint8_t idx) {
  if (idx >= SLOT_COUNT) idx = 0;
  currentSlot = idx;
  prefs.putUChar("cur", currentSlot);
}

uint8_t getCurrentSlot() {
  return currentSlot;
}

} // namespace