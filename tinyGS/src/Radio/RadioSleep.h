#pragma once
#include "../ConfigManager/ConfigManager.h"
#include "../Radio/Radio.h"
#include "../Logger/Logger.h"
#include <esp_sleep.h>
#include "../Power/Power.h"

// Returns the radio IRQ GPIO pin for this board, or -1 if unavailable.
// SX1262/SX1268/SX1280 use L_DI01, SX1278/SX1276 use L_DI00.
inline int8_t getRadioIrqPin() {
  board_t board;
  if (!ConfigManager::getInstance().getBoardConfig(board))
    return -1;

  uint8_t pin;
  switch (board.L_radio) {
    case RADIO_SX1262:
    case RADIO_SX1268:
    case RADIO_SX1280:
      pin = board.L_DI01;
      break;
    case RADIO_SX1278:
    case RADIO_SX1276:
      pin = board.L_DI00;
      break;
    default:
      return -1;
  }
  return (pin == UNUSED) ? -1 : (int8_t)pin;
}

// Configure ext0 wakeup on the radio IRQ pin (HIGH = packet received).
// Returns true if configured, false if no suitable pin.
inline bool configureRadioWakeup() {
  int8_t pin = getRadioIrqPin();
  if (pin < 0)
    return false;
  esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, 1);
  return true;
}

// Configure ext1 wakeup on the PMU IRQ pin (LOW = AXP button press).
// Clears pending PMU IRQs first to avoid instant wake.
// Returns true if configured, false if no pin or pin already LOW.
inline bool configurePmuWakeup() {
  int8_t pin = Power::getInstance().getPmuIrqPin();
  if (pin < 0)
    return false;
  // Clear any pending PMU IRQs before configuring ext1
  Power::getInstance().checkPmuStatus(true);
  // If pin is still LOW after clearing, disable ext1 (would cause instant wake)
  if (digitalRead(pin) == LOW) {
    Log::console(PSTR("AutoLP: PMU IRQ still low after clear, skipping ext1"));
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
    return false;
  }
  esp_sleep_enable_ext1_wakeup(1ULL << pin, ESP_EXT1_WAKEUP_ANY_LOW);
  return true;
}
