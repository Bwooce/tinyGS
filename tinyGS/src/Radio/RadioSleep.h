#pragma once
#include "../ConfigManager/ConfigManager.h"
#include "../Radio/Radio.h"
#include "../Logger/Logger.h"
#include <esp_sleep.h>
#include <driver/gpio.h>
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
// Skips if DIO is already HIGH (would cause immediate wake).
// Returns true if configured, false if skipped.
inline bool configureRadioWakeup() {
  int8_t pin = getRadioIrqPin();
  if (pin < 0)
    return false;
  if (digitalRead(pin) == HIGH) {
    Log::console(PSTR("AutoLP: radio DIO%d already HIGH, skipping ext0"), pin);
    return false;
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, 1);
  return true;
}

// Configure ext1 wakeup on the BOOT button (GPIO 0, active LOW).
// Enables pull-up to prevent false triggers during sleep.
// Returns true if configured, false if button already pressed.
inline bool configureButtonWakeup() {
  board_t board;
  if (!ConfigManager::getInstance().getBoardConfig(board))
    return false;
  uint8_t pin = board.PROG__BUTTON;
  if (pin == UNUSED)
    return false;
  gpio_pullup_en((gpio_num_t)pin);
  gpio_pulldown_dis((gpio_num_t)pin);
  if (digitalRead(pin) == LOW) {
    Log::console(PSTR("AutoLP: BOOT button held, skipping ext1"));
    return false;
  }
  esp_sleep_enable_ext1_wakeup(1ULL << pin, ESP_EXT1_WAKEUP_ANY_LOW);
  return true;
}
