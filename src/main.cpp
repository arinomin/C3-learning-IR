// File: src/main.cpp
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Wire.h>
#include <driver/gpio.h>

#include "config.h"
#include "slot_store.h"
#include "buttons.h"
#include "ui.h"

// 状態定義
enum class AppState : uint8_t {
  SendMode,
  LearnSelect,
  LearnMenu,
  LearnWait,
  LearnSaveConfirm,
  LearnDeleteConfirm
};

// ──────────────────────────────────
// グローバル
// ──────────────────────────────────
const uint16_t kCaptureBufferSize = 1024; // 長大なエアコン信号等に対応
const uint8_t kTimeout = 50;              // タイムアウト判定を延長(デフォルト15)

static IRsend irsend(PIN_IR_SEND);
static IRrecv irrecv(PIN_IR_RECV, kCaptureBufferSize, kTimeout, true);
static decode_results recvResults;

static Button btnSwitch(PIN_BTN_SWITCH);
static Button btnSend(PIN_BTN_SEND);

static AppState state = AppState::SendMode;
static uint8_t  menuIdx = 0;          // 0=Overwrite, 1=Delete
static SlotData capturedData;
static uint32_t stateEnterMs = 0;
static uint32_t lastUiUpdate = 0;
static uint32_t sentFlashUntil = 0;
static uint32_t blinkLastToggle = 0;
static bool     blinkOn = false;

// --- ディープスリープ＆名前選択用 ---
static uint32_t lastInteractionMs = 0;
constexpr uint32_t SLEEP_TIMEOUT_MS = 15000; // 15秒間操作なしでスリープ
static uint8_t  deviceNameIdx = 0;
const char* DEVICE_NAMES[] = {"TV", "AirCon", "Light", "Fan", "Audio", "Custom"};
constexpr uint8_t DEVICE_NAME_COUNT = 6;

// ──────────────────────────────────
// ヘルパー
// ──────────────────────────────────

static void setStatusLed(bool on) {
  digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
}

static void enterState(AppState s) {
  state = s;
  stateEnterMs = millis();
  menuIdx = 0;
}

static void sendCurrentSlot() {
  uint8_t cur = SlotStore::getCurrentSlot();
  const SlotData& d = SlotStore::get(cur);
  if (!d.used) {
    Serial.println("Empty slot, nothing to send.");
    return;
  }
  // 状態LED一瞬光らせる
  setStatusLed(true);

  // ライブラリの汎用送信。プロトコル別にAPIが分かれているが、
  // 多くは sendNEC / sendSony / sendPanasonic64 などで送信可能。
  bool ok = false;
  switch (d.protocol) {
    case decode_type_t::NEC:
    case decode_type_t::NEC_LIKE:
      irsend.sendNEC(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::SONY:
      irsend.sendSony(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::PANASONIC:
      irsend.sendPanasonic64(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::JVC:
      irsend.sendJVC(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::SAMSUNG:
      irsend.sendSAMSUNG(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::LG:
      irsend.sendLG(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::RC5:
      irsend.sendRC5(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::RC6:
      irsend.sendRC6(d.value, d.bits);
      ok = true;
      break;
    case decode_type_t::SHARP:
      irsend.sendSharpRaw(d.value, d.bits);
      ok = true;
      break;
    default:
      // ライブラリの汎用送信を試す
      if (d.protocol > decode_type_t::UNKNOWN &&
          d.protocol < decode_type_t::kLastDecodeType) {
        ok = irsend.send(d.protocol, d.value, d.bits);
      }
      break;
  }

  Serial.printf("Sent slot=%u proto=%s value=0x%llX bits=%u (ok=%d)\n",
                cur, d.name.c_str(),
                (unsigned long long)d.value, d.bits, ok);

  delay(60);
  setStatusLed(false);
  sentFlashUntil = millis() + SENT_FLASH_MS;
}

static bool tryDecodeAsSlotData(SlotData& out) {
  if (recvResults.decode_type == decode_type_t::UNKNOWN) return false;
  if (recvResults.bits == 0) return false;
  out.used     = true;
  out.protocol = recvResults.decode_type;
  out.value    = recvResults.value;
  out.bits     = recvResults.bits;
  out.name     = typeToString(recvResults.decode_type);
  return true;
}

// ──────────────────────────────────
// 各状態のハンドラ
// ──────────────────────────────────

static void handleSendMode(ButtonEvent eSw, ButtonEvent eSn) {
  if (eSw == ButtonEvent::ShortPress) {
    uint8_t cur = SlotStore::getCurrentSlot();
    cur = (cur + 1) % SLOT_COUNT;
    SlotStore::setCurrentSlot(cur);
  } else if (eSw == ButtonEvent::LongPress) {
    enterState(AppState::LearnSelect);
    return;
  }
  if (eSn == ButtonEvent::ShortPress) {
    sendCurrentSlot();
  }
}

static void handleLearnSelect(ButtonEvent eSw, ButtonEvent eSn) {
  if (eSw == ButtonEvent::ShortPress) {
    uint8_t cur = SlotStore::getCurrentSlot();
    cur = (cur + 1) % SLOT_COUNT;
    SlotStore::setCurrentSlot(cur);
  } else if (eSw == ButtonEvent::LongPress) {
    enterState(AppState::SendMode);
    return;
  }
  if (eSn == ButtonEvent::ShortPress) {
    uint8_t cur = SlotStore::getCurrentSlot();
    const SlotData& d = SlotStore::get(cur);
    if (d.used) {
      enterState(AppState::LearnMenu);
    } else {
      irrecv.resume();
      enterState(AppState::LearnWait);
    }
  }
}

static void handleLearnMenu(ButtonEvent eSw, ButtonEvent eSn) {
  if (eSw == ButtonEvent::ShortPress) {
    menuIdx = (menuIdx + 1) % 2;
  } else if (eSw == ButtonEvent::LongPress) {
    enterState(AppState::LearnSelect);
    return;
  }
  if (eSn == ButtonEvent::ShortPress) {
    if (menuIdx == 0) {
      irrecv.resume();
      enterState(AppState::LearnWait);
    } else {
      enterState(AppState::LearnDeleteConfirm);
    }
  }
  // タイムアウト
  if (millis() - stateEnterMs > MENU_TIMEOUT_MS) {
    enterState(AppState::LearnSelect);
  }
}

static void handleLearnWait(ButtonEvent eSw, ButtonEvent eSn) {
  if (eSw == ButtonEvent::ShortPress || eSw == ButtonEvent::LongPress) {
    enterState(AppState::SendMode);
    return;
  }
  if (irrecv.decode(&recvResults)) {
    SlotData d;
    if (tryDecodeAsSlotData(d)) {
      capturedData = d;
      // ★初期値を「解析されたプロトコル名(NEC等)」にする
      // deviceNameIdx = 0 をプロトコル名とし、1〜6をDEVICE_NAMESにする
      deviceNameIdx = 0; 
      
      Serial.printf("Captured: %s 0x%llX bits=%u\n", 
                    capturedData.name.c_str(),
                    (unsigned long long)capturedData.value, capturedData.bits);
      enterState(AppState::LearnSaveConfirm);
    } else {
      irrecv.resume();
    }
    return;
  }
  if (millis() - stateEnterMs > LEARN_TIMEOUT_MS) {
    enterState(AppState::SendMode);
  }
}

static void handleLearnSaveConfirm(ButtonEvent eSw, ButtonEvent eSn) {
  if (eSn == ButtonEvent::ShortPress) {
    uint8_t cur = SlotStore::getCurrentSlot();
    SlotStore::save(cur, capturedData);
    Serial.printf("Saved to slot %u with name: %s\n", cur, capturedData.name.c_str());
    enterState(AppState::LearnSelect);
  } else if (eSw == ButtonEvent::ShortPress) {
    // ★Switchボタンで名前をサイクル
    // 0: 解析されたプロトコル名, 1〜DEVICE_NAME_COUNT: プリセット名
    deviceNameIdx = (deviceNameIdx + 1) % (DEVICE_NAME_COUNT + 1);
    
    if (deviceNameIdx == 0) {
      // 解析結果のプロトコル名（NEC, PANASONIC, SONY等）を再取得
      capturedData.name = typeToString(capturedData.protocol);
    } else {
      // プリセット名（TV, AirCon等）を選択
      capturedData.name = DEVICE_NAMES[deviceNameIdx - 1];
    }
  } else if (eSw == ButtonEvent::LongPress) {
    irrecv.resume();
    enterState(AppState::LearnWait);
  }
}

static void handleLearnDeleteConfirm(ButtonEvent eSw, ButtonEvent eSn) {
  if (eSn == ButtonEvent::ShortPress) {
    uint8_t cur = SlotStore::getCurrentSlot();
    SlotStore::clear(cur);
    Serial.printf("Cleared slot %u\n", cur);
    enterState(AppState::LearnSelect);
  } else if (eSw == ButtonEvent::ShortPress) {
    enterState(AppState::LearnMenu);
  }
}

// ──────────────────────────────────
// 描画とLED更新
// ──────────────────────────────────

static void updateUi() {
  uint8_t cur = SlotStore::getCurrentSlot();
  const SlotData& d = SlotStore::get(cur);

  switch (state) {
    case AppState::SendMode: {
      bool flashing = millis() < sentFlashUntil;
      UI::showSendMode(cur, d, flashing);
      break;
    }
    case AppState::LearnSelect:
      UI::showLearnSelect(cur, d);
      break;
    case AppState::LearnMenu:
      UI::showLearnMenu(cur, d, menuIdx);
      break;
    case AppState::LearnWait: {
      uint32_t elapsed = millis() - stateEnterMs;
      uint32_t remain  = (LEARN_TIMEOUT_MS > elapsed)
                          ? (LEARN_TIMEOUT_MS - elapsed) / 1000
                          : 0;
      UI::showLearnWait(cur, remain);
      break;
    }
    case AppState::LearnSaveConfirm:
      UI::showLearnSaveConfirm(capturedData);
      break;
    case AppState::LearnDeleteConfirm:
      UI::showLearnDeleteConfirm(cur, d);
      break;
  }
}

static void updateStatusLed() {
  // 送信フラッシュ中は handleSend 側で制御済み（既に消えている）
  if (millis() < sentFlashUntil) return;

  switch (state) {
    case AppState::SendMode:
      setStatusLed(false);
      break;
    case AppState::LearnSelect:
    case AppState::LearnMenu:
    case AppState::LearnSaveConfirm:
    case AppState::LearnDeleteConfirm:
      setStatusLed(true);  // 学習モード中は常時点灯
      break;
    case AppState::LearnWait: {
      // 点滅
      uint32_t now = millis();
      if (now - blinkLastToggle >= LEARN_BLINK_MS) {
        blinkLastToggle = now;
        blinkOn = !blinkOn;
        setStatusLed(blinkOn);
      }
      break;
    }
  }
}
static void goToDeepSleep() {
  Serial.println("Going to Deep Sleep...");
  UI::showMessage("Sleep", "Good bye.");
  delay(1000);
  
  // OLEDをコマンドで消灯
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(0xAE); // Display OFF
  Wire.endTransmission();

  // ★青色LEDを完全に消灯し、スリープ中もLOW状態を強制維持する（漏れ電流防止）
  digitalWrite(PIN_STATUS_LED, LOW);
  gpio_hold_en((gpio_num_t)PIN_STATUS_LED);
  gpio_deep_sleep_hold_en();

  // ★どちらのボタンが押されても復帰できるようにビットマスクを合成
  pinMode(PIN_BTN_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BTN_SEND, INPUT_PULLUP);
  uint64_t wakeMask = (1ULL << PIN_BTN_SWITCH) | (1ULL << PIN_BTN_SEND);
  esp_deep_sleep_enable_gpio_wakeup(wakeMask, ESP_GPIO_WAKEUP_GPIO_LOW);
  
  esp_deep_sleep_start();
}
// ──────────────────────────────────
// setup / loop
// ──────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_STATUS_LED, OUTPUT);
  setStatusLed(false);

  btnSwitch.begin();
  btnSend.begin();

  SlotStore::begin();
  SlotStore::loadAll();

  UI::begin();
  UI::showMessage("IR Learning Remote", "Booting...");
  delay(500);

  irsend.begin();
  irrecv.enableIRIn();

  enterState(AppState::SendMode);
  Serial.println("Ready.");
}

void loop() {
  ButtonEvent eSw = btnSwitch.update();
  ButtonEvent eSn = btnSend.update();

  // ★いずれかのボタン操作があればスリープタイマーをリセット
  if (eSw != ButtonEvent::None || eSn != ButtonEvent::None) {
    lastInteractionMs = millis();
  }

  AppState before = state;
  switch (state) {
    case AppState::SendMode:           handleSendMode(eSw, eSn);          break;
    case AppState::LearnSelect:        handleLearnSelect(eSw, eSn);       break;
    case AppState::LearnMenu:          handleLearnMenu(eSw, eSn);         break;
    case AppState::LearnWait:          handleLearnWait(eSw, eSn);         break;
    case AppState::LearnSaveConfirm:   handleLearnSaveConfirm(eSw, eSn);  break;
    case AppState::LearnDeleteConfirm: handleLearnDeleteConfirm(eSw, eSn);break;
  }

  // ★30秒放置でディープスリープへ（SendMode時のみ動作）
  if (state == AppState::SendMode && (millis() - lastInteractionMs > SLEEP_TIMEOUT_MS)) {
    goToDeepSleep();
  }

  // UIは最大20Hzで更新（OLEDのI2C負荷軽減）
  uint32_t now = millis();
  if (state != before || now - lastUiUpdate > 50) {
    updateUi();
    lastUiUpdate = now;
  }

  updateStatusLed();
  delay(2);
}