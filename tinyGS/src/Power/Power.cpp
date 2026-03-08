/*
  Power.cpp - Class responsible of controlling the display

  Copyright (C) 2020 -2024  Megazaic39 [E16] , @G4lile0, @gmag12 and @dev_4m1g0

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include "Power.h"
#include "../Logger/Logger.h"

#if defined(ESP32)

byte AXPchip = 0;
byte pmustat1;
byte pmustat2;
byte pwronsta;
byte pwrofsta;
byte irqstat0;
byte irqstat1;
byte irqstat2;

static volatile bool pmuIrqFired = false;
static volatile uint32_t pmuIsrCount = 0;

void IRAM_ATTR Power::pmuIrqHandler() {
    pmuIrqFired = true;
    pmuIsrCount++;
}

#define LEGACY_BATT_PIN 36

// All register defines, bitmasks, and IRQ bit definitions are in Power.h

Power::Power() : pmuWire(&Wire) {}

void Power::I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data)
{
  pmuWire->beginTransmission(Address);
  pmuWire->write(Register);
  pmuWire->write(Data);
  pmuWire->endTransmission();
}

uint8_t Power::I2CreadByte(uint8_t Address, uint8_t Register)
{
  uint8_t Nbytes = 1;
  pmuWire->beginTransmission(Address);
  pmuWire->write(Register);
  pmuWire->endTransmission();
  pmuWire->requestFrom(Address, Nbytes);
  byte slaveByte = pmuWire->read();
  pmuWire->endTransmission();
  return slaveByte;
}

void Power::I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data)
{
  pmuWire->beginTransmission(Address);
  pmuWire->write(Register);
  pmuWire->endTransmission();
  pmuWire->requestFrom(Address, Nbytes);
  uint8_t index = 0;
  while (pmuWire->available())
    Data[index++] = pmuWire->read();
}


void Power::checkAXP()
{
   board_t board;
   uint8_t boardIdx = ConfigManager::getInstance().getBoard();
   if (!ConfigManager::getInstance().getBoardConfig(board))
    return;
  Log::console(PSTR("AXPxxx chip?"));
  byte regV = 0;

#if CONFIG_IDF_TARGET_ESP32S3
  if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
      Log::console(PSTR("PMU: Using Supreme Wire1 on pins %d/%d"), SUPREME_PMU_SDA, SUPREME_PMU_SCL);
      Wire1.begin(SUPREME_PMU_SDA, SUPREME_PMU_SCL);
      pmuWire = &Wire1;
  } else
#endif
  {
      Wire.begin(board.OLED__SDA, board.OLED__SCL);                     // I2C_SDA, I2C_SCL on all new boards
      pmuWire = &Wire;
  }

  byte ChipID = I2CreadByte(AXP_SLAVE_ADDRESS, AXP192_CHIP_ID);                            // read byte from IC_TYPE register (0x03)
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  if (ChipID == XPOWERS_AXP192_CHIP_ID) { // 0x03
    AXPchip = 1;
    Log::console(PSTR("AXP192 found"));   // T-Beam V1.1 with AXP192 power controller
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_LDO23_OUT_VOL, 0xFF);       // Set LDO2 (LoRa) & LDO3 (GPS) to 3.3V , (1.8-3.3V, 100mV/step)
    regV = I2CreadByte(AXP192_SLAVE_ADDRESS, 0x12);       // Power Output Control
    regV = regV | 0x0C;                   // set bit 2 (LDO2) and bit 3 (LDO3)
    I2CwriteByte(AXP192_SLAVE_ADDRESS, 0x12, regV);       // and power channels now enabled
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_ADC_SPEED, 0b11000010); // Set ADC sample rate to 200hz, TS pin control 20uA
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_ADC_EN1, 0xFF);       // |Set ADC to|
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_ADC_EN2, 0x80);       // |All Enable|
    // Battery pack dependent charge config (reg 0x33 bit7=enable, bits[6:5]=4.2V target, bits[3:0]=current)
    {
      uint8_t bp = ConfigManager::getInstance().getBatteryPack();
      switch (bp) {
        case 2:
          I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_CHARGE1, 0xC7); // 4.2V, 700mA
          I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_CHARGE2, 0x83); // precharge 50m, CC timeout 10h
          Log::console(PSTR("PMU: Battery 2P config - 700mA charge, 10h/50m timeouts"));
          break;
        case 3:
          I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_CHARGE1, 0xCB); // 4.2V, 1000mA
          I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_CHARGE2, 0xC3); // precharge 60m, CC timeout 10h
          Log::console(PSTR("PMU: Battery 3P config - 1000mA charge, 10h/60m timeouts"));
          break;
        default: // 1P
          I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_BAT_CHG_DIG_VOL, 0xC3); // 4.2V, 360mA, 10% termination
          Log::console(PSTR("PMU: Battery 1P config - 360mA charge, default timeouts"));
          break;
      }
    }
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_PEK_SET, 0x0C);       // 128ms power on, 4s power off, 1s Long key press
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_VBUS_VOL_LIMIT, 0x80);       // Disable VBUS limits
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_ADAPTER_OT_SET, 0xFC);       // Set TS protection to 3.2256v -> disable
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_CHGLED_CONTROL, 0x46);       // CHGLED controlled by the charging function
    pmustat1 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_STATUS);
    pmustat2 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_MODE_CHGSTATUS);
    irqstat0 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS1);
    irqstat1 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS2);
    irqstat2 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS3);
    Log::console(PSTR("PMU status1,status2 : %02X,%02X"), pmustat1, pmustat2);
    { char irqDesc[64]; uint8_t irqs[3] = {irqstat0, irqstat1, irqstat2};
      decodeIRQs(1, irqs, irqDesc, sizeof(irqDesc));
      Log::console(PSTR("IRQ status 1,2,3    : %02X,%02X,%02X [%s]"), irqstat0, irqstat1, irqstat2, irqDesc);
    }
  }
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  if (ChipID == XPOWERS_AXP2101_CHIP_ID) {// 0x4A
    AXPchip = 2;
    Log::console(PSTR("AXP2101 found"));  // T-Beam V1.2 or Supreme with AXP2101 power controller

#if CONFIG_IDF_TARGET_ESP32S3
    if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
      // T-Beam Supreme
      Log::console(PSTR("Configuring for T-Beam Supreme"));
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO1_VOLT, 0x1C);       // set ALDO1 voltage to 3.3V ( Display )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO3_VOLT, 0x1C);       // set ALDO3 voltage to 3.3V ( Radio )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO4_VOLT, 0x1C);       // set ALDO4 voltage to 3.3V ( GPS )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_BAT_CHG_BACKUP, 0x04);       // set Button battery voltage to 3.0V ( backup battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CV_VOLT, 0x03);       // set Main battery voltage to 4.2V ( 18650 battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ITERM, 0x15);       // set Main battery term charge current to 125mA

      // Battery pack dependent charge config
      {
        uint8_t bp = ConfigManager::getInstance().getBatteryPack();
        switch (bp) {
          case 2:
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x06);               // precharge 150mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC_CHG_SET, 0x0E); // charge current 800mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_TIMEOUT_CTRL, 0xF6); // slow_DPM, 20h CC, 60m pre
            Log::console(PSTR("PMU: Battery 2P config - 800mA charge, 20h/60m timeouts"));
            break;
          case 3:
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x07);               // precharge 175mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC_CHG_SET, 0x10); // charge current 1000mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_TIMEOUT_CTRL, 0xF7); // slow_DPM, 20h CC, 70m pre
            Log::console(PSTR("PMU: Battery 3P config - 1000mA charge, 20h/70m timeouts"));
            break;
          default: // 1P
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x05);               // precharge 125mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC_CHG_SET, 0x0A); // charge current 400mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_TIMEOUT_CTRL, 0xE5); // slow_DPM, 12h CC, 50m pre
            Log::console(PSTR("PMU: Battery 1P config - 400mA charge, 12h/50m timeouts"));
            break;
        }
      }

      // Explicitly disable unused rails (ALDO2, BLDO1/2, DLDO1/2)
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL1, 0x00); // Disable BLDO1, BLDO2, DLDO1, DLDO2

      regV = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0);
      regV &= ~(1 << AXP2101_ALDO2_BIT); // Disable ALDO2
      regV = regV | (1 << AXP2101_ALDO1_BIT) | (1 << AXP2101_ALDO3_BIT) | (1 << AXP2101_ALDO4_BIT);
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0, regV);       // and power channels now enabled
    } else
#endif
    {
      // T-Beam V1.2 (SDA likely 21)
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO2_VOLT, 0x1C);       // set ALDO2 voltage to 3.3V ( LoRa VCC )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO3_VOLT, 0x1C);       // set ALDO3 voltage to 3.3V ( GPS VDD )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_BAT_CHG_BACKUP, 0x04);       // set Button battery voltage to 3.0V ( backup battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CV_VOLT, 0x03);       // set Main battery voltage to 4.2V ( 18650 battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ITERM, 0x15);       // set Main battery term charge current to 125mA

      // Battery pack dependent charge config
      {
        uint8_t bp = ConfigManager::getInstance().getBatteryPack();
        switch (bp) {
          case 2:
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x06);               // precharge 150mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC_CHG_SET, 0x0E); // charge current 800mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_TIMEOUT_CTRL, 0xF6); // slow_DPM, 20h CC, 60m pre
            Log::console(PSTR("PMU: Battery 2P config - 800mA charge, 20h/60m timeouts"));
            break;
          case 3:
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x07);               // precharge 175mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC_CHG_SET, 0x10); // charge current 1000mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_TIMEOUT_CTRL, 0xF7); // slow_DPM, 20h CC, 70m pre
            Log::console(PSTR("PMU: Battery 3P config - 1000mA charge, 20h/70m timeouts"));
            break;
          default: // 1P
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x05);               // precharge 125mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC_CHG_SET, 0x0A); // charge current 400mA
            I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_TIMEOUT_CTRL, 0xE5); // slow_DPM, 12h CC, 50m pre
            Log::console(PSTR("PMU: Battery 1P config - 400mA charge, 12h/50m timeouts"));
            break;
        }
      }
      regV = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0);
      regV = regV | (1 << AXP2101_ALDO2_BIT) | (1 << AXP2101_ALDO3_BIT);
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0, regV);       // and power channels now enabled
    }

    regV = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
    regV = regV | 0x06;                   // set bit 1 (Main Battery) and bit 2 (Button battery)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, regV);       // and chargers now enabled
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_BAT_V_LIMIT, 0x30);       // set minimum system voltage to 4.4V (default 4.7V)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_VBUS_V_LIMIT, 0x09);       // VINDPM 4.60V - throttles charge current before solar panel regulator dropout
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, 0x16, 0x05);                       // input current limit 2000mA - VINDPM prevents over-draw
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_VOFF_SET, 0x06);       // set Vsys for PWROFF threshold to 3.2V
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_TS_PIN_CTRL, 0x14);       // set TS pin to EXTERNAL input (not temperature)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHGLED_SET, 0x01);       // set CHGLED for 'type A' and enable pin function
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_PEK_SET, 0x00);       // set IRQLevel/OFFLevel/ONLevel to minimum (1S/4S/128mS)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ADC_CONFIG, 0xFF);       // enable ADC for SYS, VBUS, TS and Battery
    // Configure IRQ enables: disable noisy sources (GAUGE_NEW_SOC, WDT, CHG_START, PKEY edges)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_ENABLE0, 0xCF);  // bat temp, SOC warn levels
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_ENABLE1, 0xFC);  // VBUS, battery, button short/long press
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_ENABLE2, 0x77);  // bat OV, charger timer, die OT, chg done, overcurrent
    // PMU watchdog: 128s timeout, full power cycle on expiry (DCDC/LDO off + PWRON)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL, 0x37);   // action=3 (bits[5:4]), timeout=7/128s (bits[2:0])
    { uint8_t reg18 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, reg18 | 0x01); } // enable WDT (bit 0)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL,
        I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL) | 0x08); // feed WDT (bit 3)
    Log::console(PSTR("PMU: watchdog enabled (128s, full power cycle)"));
    pmustat1 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS1);
    pmustat2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS2);
    pwronsta = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_PWRON_STATUS);
    pwrofsta = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_PWROFF_STATUS);
    irqstat0 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0);
    irqstat1 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS1);
    irqstat2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS2);
    Log::console(PSTR("PMU status1,status2 : %02X,%02X"), pmustat1, pmustat2);
    Log::console(PSTR("PWRON,PWROFF status : %02X,%02X"), pwronsta, pwrofsta);
    { char irqDesc[64]; uint8_t irqs[3] = {irqstat0, irqstat1, irqstat2};
      decodeIRQs(2, irqs, irqDesc, sizeof(irqDesc));
      Log::console(PSTR("IRQ status 0,1,2    : %02X,%02X,%02X [%s]"), irqstat0, irqstat1, irqstat2, irqDesc);
      // Clear IRQ status (write-1-to-clear) so bits don't persist across boots
      if (irqstat0) I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0, irqstat0);
      if (irqstat1) I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS1, irqstat1);
      if (irqstat2) I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS2, irqstat2);
    }
    { uint8_t en0 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_ENABLE0);
      uint8_t en1 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_ENABLE1);
      uint8_t en2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_ENABLE2);
      Log::console(PSTR("IRQ enable 0,1,2    : %02X,%02X,%02X"), en0, en1, en2);
    }

#if CONFIG_IDF_TARGET_ESP32S3
    if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
        deepSleepSensors();
    }
#endif
  }
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

  // Attach PMU IRQ interrupt (active-low, open-drain from AXP)
  if (AXPchip) {
#if CONFIG_IDF_TARGET_ESP32S3
    if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
        pmuIrqPin = SUPREME_PMU_IRQ;
    }
#else
    if (boardIdx == TBEAM_OLED_LF || boardIdx == TBEAM_OLED_HF ||
        boardIdx == TBEAM_OLED_v1_0 || boardIdx == TBEAM_OLED_v1_0_HF) {
        pmuIrqPin = TBEAM_PMU_IRQ;
    }
#endif
    if (pmuIrqPin >= 0) {
        pinMode(pmuIrqPin, INPUT_PULLUP);
        clearIRQ();
        pmuIrqFired = false;
        attachInterrupt(digitalPinToInterrupt(pmuIrqPin), pmuIrqHandler, FALLING);
        Log::console(PSTR("PMU: IRQ on GPIO %d"), pmuIrqPin);
    }
  }

  Wire.end();
}

void Power::deepSleepSensors() {
    board_t board;
    if (!ConfigManager::getInstance().getBoardConfig(board)) return;

    // Initialize primary Wire bus for sensors/OLED (pins 17/18 on Supreme)
    Wire.begin(board.OLED__SDA, board.OLED__SCL);

    // Check for QMC5883L (Compass) at 0x0D
    Wire.beginTransmission(0x0D);
    if (Wire.endTransmission() == 0) {
        Log::console(PSTR("Found QMC5883L at 0x0D, putting to sleep..."));
        Wire.beginTransmission(0x0D);
        Wire.write(0x09); // Control Register 1
        Wire.write(0x00); // Standby Mode
        Wire.endTransmission();
    }

    // Check for BME280 (Environment) at 0x77
    Wire.beginTransmission(0x77);
    if (Wire.endTransmission() == 0) {
        Log::console(PSTR("Found BME280 at 0x77, putting to sleep..."));
        Wire.beginTransmission(0x77);
        Wire.write(0xF4); // CTRL_MEAS register
        Wire.write(0x00); // Sleep mode
        Wire.endTransmission();
    }
}

float Power::getBatteryVoltage() {
    board_t board;
    if (!ConfigManager::getInstance().getBoardConfig(board)) return 0;

    float voltage = 0;

    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_BAT_AVERVOL_H, 2, buf);
        voltage = ((buf[0] << AXP192_ADC_MSB_SHIFT) | (buf[1] & AXP192_ADC_LSB_MASK)) * AXP192_BAT_VOL_STEP;
    } else if (AXPchip == 2) { // AXP2101
        uint8_t buf[2] = {0, 0};
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_BATTERY_VOLT_H, 2, buf);
        // AXP2101: Voltage is 16-bit (H << 8 | L), 1mV/bit
        voltage = (float)((buf[0] << AXP2101_VOLT_MSB_SHIFT) | buf[1]);
    } else {
        // Fallback for boards without PMU (Heltec V1/V2 etc uses GPIO 36)
        int length = 21;
        int voltages[22];

        for (int i = 0; i < 21; i++) {
            voltages[i] = analogRead(LEGACY_BATT_PIN);
        }

        // BubbleSortAsc
        int i, j, flag = 1;
        int temp;
        for (i = 1; (i <= length) && flag; i++) {
            flag = 0;
            for (j = 0; j < (length - 1); j++) {
                if (voltages[j + 1] < voltages[j]) {
                    temp = voltages[j];
                    voltages[j] = voltages[j + 1];
                    voltages[j + 1] = temp;
                    flag = 1;
                }
            }
        }
        voltage = (float)voltages[10];
    }
    return voltage;
}

float Power::getVbusVoltage() {
    board_t board;
    if (!ConfigManager::getInstance().getBoardConfig(board)) return 0;

    float voltage = 0;
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2] = {0, 0};
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_VBUS_VOL_H, 2, buf);
        voltage = ((buf[0] << AXP192_ADC_MSB_SHIFT) | (buf[1] & AXP192_ADC_LSB_MASK)) * AXP192_VBUS_VOL_STEP;
    } else if (AXPchip == 2) { // AXP2101
        uint8_t buf[2] = {0, 0};
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_VBUS_VOLT_H, 2, buf);
        // AXP2101 VBUS: 16-bit (H << 8 | L), 1mV/bit
        voltage = (float)((buf[0] << AXP2101_VOLT_MSB_SHIFT) | buf[1]);
    }
    return voltage;
}

float Power::getVbusCurrent() {
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_VBUS_CUR_H, 2, buf);
        return ((buf[0] << AXP192_ADC_MSB_SHIFT) | (buf[1] & AXP192_ADC_LSB_MASK)) * AXP192_VBUS_CUR_STEP;
    }
    // AXP2101 has no VBUS current ADC
    return 0;
}

float Power::getVsysVoltage() {
    if (AXPchip == 2) { // AXP2101 only
        uint8_t buf[2] = {0, 0};
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_VSYS_VOLT_H, 2, buf);
        // VSYS voltage: 14-bit (bits 13:0), 1mV/bit
        return (float)(((buf[0] & AXP2101_ADC_MSB_MASK) << 8) | buf[1]);
    }
    return 0;
}

float Power::getBatteryChargeCurrent() {
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_BAT_AVERCHGCUR_H, 2, buf);
        return ((buf[0] << AXP192_CUR_MSB_SHIFT) | (buf[1] & AXP192_CUR_LSB_MASK)) * AXP192_CUR_STEP;
    }
    // AXP2101 has no battery current ADC
    return 0;
}

float Power::getBatteryDischargeCurrent() {
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_BAT_AVERDISCHGCUR_H, 2, buf);
        return ((buf[0] << AXP192_CUR_MSB_SHIFT) | (buf[1] & AXP192_CUR_LSB_MASK)) * AXP192_CUR_STEP;
    }
    // AXP2101 has no battery current ADC
    return 0;
}

float Power::getSystemCurrent() {
    if (AXPchip == 1) { // AXP192 only
        float vbus = getVbusCurrent();
        float batt = getBatteryCurrent();
        return vbus - batt;
    }
    // AXP2101 has no current measurement ADCs
    return 0;
}

float Power::getDieTemperature() {
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_DIE_TEMP_H, 2, buf);
        uint16_t raw = (buf[0] << AXP192_ADC_MSB_SHIFT) | (buf[1] & AXP192_ADC_LSB_MASK);
        return raw * AXP192_DIE_TEMP_STEP + AXP192_DIE_TEMP_OFFSET;
    } else if (AXPchip == 2) { // AXP2101
        uint8_t buf[2];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_DIE_TEMP_H, 2, buf);
        uint16_t raw = ((buf[0] & AXP2101_DIE_TEMP_MSB_MASK) << 8) | buf[1];
        return AXP2101_DIE_TEMP_BASE + (AXP2101_DIE_TEMP_CENTER - raw) / AXP2101_DIE_TEMP_DIVISOR;
    }
    return 0;
}

int Power::getBatteryPercentage() {
    board_t board;
    if (!ConfigManager::getInstance().getBoardConfig(board)) return 0;

    int pct = 0;

    // Safety check: if voltage is 0, percentage must be 0 (ignore fuel gauge or recalc)
    float v = getBatteryVoltage();
    if (v < 100) return 0; // Voltage is in mV, so < 0.1V is effectively 0

    if (AXPchip == 2) { // AXP2101 has fuel gauge
        pct = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_FUEL_GAUGE);
        if (pct > 100) pct = 0; // Invalid
    } else {
        // Simple voltage based approx for others
        if (v > 4100) pct = 100;
        else if (v < 3300) pct = 0;
        else pct = (v - 3300) / (4100 - 3300) * 100;
    }
    return pct;
}

uint8_t Power::getChipType() {
    return AXPchip;
}

void Power::setGnssPower(bool on) {
    board_t board;
    if (!ConfigManager::getInstance().getBoardConfig(board))
        return;

    if (AXPchip == 1) { // AXP192
        uint8_t reg = I2CreadByte(AXP192_SLAVE_ADDRESS, 0x12);
        if (on) reg |= (1 << 3); else reg &= ~(1 << 3); // LDO3
        I2CwriteByte(AXP192_SLAVE_ADDRESS, 0x12, reg);
    } else if (AXPchip == 2) { // AXP2101
        uint8_t reg = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0);
        uint8_t boardIdx = ConfigManager::getInstance().getBoard();

#if CONFIG_IDF_TARGET_ESP32S3
        if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) { // Supreme
             if (on) reg |= (1 << AXP2101_ALDO4_BIT);
             else reg &= ~(1 << AXP2101_ALDO4_BIT);
        } else
#endif
        {
             if (on) reg |= (1 << AXP2101_ALDO3_BIT); else reg &= ~(1 << AXP2101_ALDO3_BIT);
        }
        I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0, reg);
    }
}

bool Power::isVbusPresent() {
    if (AXPchip == 1 || AXPchip == 2) {
        uint8_t reg = I2CreadByte(AXP_SLAVE_ADDRESS, AXP2101_STATUS1); // Common STATUS register (0x00)
        return (reg & (1 << AXP2101_VBUS_PRESENT_BIT)) != 0;
    }
    return false;
}

bool Power::isCharging() {
    if (AXPchip == 1) {
        uint8_t reg = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_MODE_CHGSTATUS);
        return (reg & (1 << 6)) != 0;
    } else if (AXPchip == 2) {
        uint8_t reg = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS2);
        uint8_t status = reg & AXP2101_CHG_STATUS_MASK;
        return (status >= 1 && status <= 4); // 1: Charge start, 2: Charge, 3: Charge end, 4: Over-temp charge
    }
    return false;
}

float Power::getBatteryCurrent() {
    if (AXPchip == 1 || AXPchip == 2) {
        // Net current = Charge current - Discharge current
        return getBatteryChargeCurrent() - getBatteryDischargeCurrent();
    }
    return 0;
}

const char* Power::getChargeStateStr() {
    if (AXPchip == 2) {
        uint8_t status2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS2);
        uint8_t chgState = status2 & AXP2101_CHG_STATUS_MASK;
        switch (chgState) {
          case 0: return "Tri-charge";
          case 1: return "Pre-charge";
          case 2: return "CC charging";
          case 3: return "CV charging";
          case 4: return "Done";
          case 5: return "Not charging";
          default: return "Unknown";
        }
    } else if (AXPchip == 1) {
        uint8_t status1 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_MODE_CHGSTATUS);
        if (status1 & 0x40) return "Charging";
        return "Not charging";
    }
    return "No PMU";
}

void Power::getPmuData(PmuData* data) {
    data->battVol = getBatteryVoltage();
    data->vbusVol = getVbusVoltage();
    data->vsysVol = getVsysVoltage();
    data->battChgCur = getBatteryChargeCurrent();
    data->battDischgCur = getBatteryDischargeCurrent();
    data->battCur = data->battChgCur - data->battDischgCur;
    data->vbusCur = getVbusCurrent();
    data->sysCur = getSystemCurrent();
    data->dieTemp = getDieTemperature();
    data->battPct = getBatteryPercentage();
    data->vbusPresent = isVbusPresent();
    data->charging = isCharging();
    // IRQs are handled by interrupt-driven checkPmuStatus(), not here
    data->irqs[0] = data->irqs[1] = data->irqs[2] = 0;

    if (AXPchip == 2) { // AXP2101
        uint8_t buf[2];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_BATTERY_VOLT_H, 2, buf);
        data->raw_battVol = (buf[0] << 8) | buf[1];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_VBUS_VOLT_H, 2, buf);
        data->raw_vbusVol = (buf[0] << 8) | buf[1];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_DIE_TEMP_H, 2, buf);
        data->raw_dieTemp = (buf[0] << 8) | buf[1];
    }
}

void Power::getIRQStatus(uint8_t* irqs) {
    if (AXPchip == 1) {
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS1, 3, irqs);
    } else if (AXPchip == 2) {
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0, 3, irqs);
    } else {
        irqs[0] = irqs[1] = irqs[2] = 0;
    }
}

void Power::clearIRQ() {
    uint8_t irqs[3];
    if (AXPchip == 1) {
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS1, 3, irqs);
        I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS1, irqs[0]);
        I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS2, irqs[1]);
        I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS3, irqs[2]);
    } else if (AXPchip == 2) {
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0, 3, irqs);
        I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0, irqs[0]);
        I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS1, irqs[1]);
        I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS2, irqs[2]);
    }
}

void Power::enableWatchdog() {
    if (AXPchip != 2) return;
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL, 0x37);  // 128s, full power cycle
    uint8_t reg18 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, reg18 | 0x01);
    feedWatchdog();
    // Verify enable took effect
    reg18 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
    if (!(reg18 & 0x01)) {
        Log::console(PSTR("PMU: watchdog enable FAILED (R18=%02X), retrying"), reg18);
        I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, reg18 | 0x01);
        feedWatchdog();
    }
}

void Power::disableWatchdog() {
    if (AXPchip != 2) return;
    uint8_t reg18 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, reg18 & ~0x01);
    // Verify disable took effect
    reg18 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
    if (reg18 & 0x01) {
        Log::console(PSTR("PMU: watchdog disable FAILED (R18=%02X), retrying"), reg18);
        I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, reg18 & ~0x01);
    }
}

void Power::feedWatchdog() {
    if (AXPchip != 2) return;
    uint8_t val = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL);
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL, val | 0x08);
}

void Power::setWatchdogSleepMode() {
    if (AXPchip != 2) return;
    // Switch to IRQ-only action (bits[5:4]=00) so expiry during sleep is harmless
    uint8_t val = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL);
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL, (val & 0xCF) | 0x08);  // clear action, feed
}

void Power::setWatchdogActiveMode() {
    if (AXPchip != 2) return;
    // Restore full power cycle action (bits[5:4]=11) and feed
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_WDT_CTRL, 0x3F);  // action=3, timeout=128s, feed
}

void Power::checkPmuStatus(bool force) {
    if (AXPchip == 0) return;

    // Feed PMU watchdog every 30s
    if (AXPchip == 2) {
        static unsigned long lastWdtFeed = 0;
        unsigned long now = millis();
        if (now - lastWdtFeed >= 30000) {
            lastWdtFeed = now;
            feedWatchdog();
        }
    }

    // IRQ-driven: handle PMU interrupts immediately
    // For boards without IRQ pin wired, poll on periodic cadence instead
    // force=true bypasses the IRQ gate (used during sleep wake handling)
    bool checkIrqs = force || pmuIrqFired;
    if (pmuIrqFired) pmuIrqFired = false;
    if (!checkIrqs) {
        // Poll IRQs every 60s as fallback (or diagnostic when ISR pin is set)
        static unsigned long lastIrqPoll = 0;
        unsigned long now = millis();
        if (now - lastIrqPoll >= 60000) {
            lastIrqPoll = now;
            checkIrqs = true;
        }
    }
    if (checkIrqs) {

        if (AXPchip == 2) {
            uint8_t irqs[3];
            I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0, 3, irqs);
            if (irqs[0]) I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0, irqs[0]);
            if (irqs[1]) I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS1, irqs[1]);
            if (irqs[2]) I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS2, irqs[2]);

            if (irqs[0] || irqs[1] || irqs[2]) {
                char irqDesc[64];
                decodeIRQs(2, irqs, irqDesc, sizeof(irqDesc));
                Log::console(PSTR("PMU IRQ: %02X,%02X,%02X [%s]"), irqs[0], irqs[1], irqs[2], irqDesc);
            }
            if (irqs[1] & AXP2101_IRQ1_PKEY_SHORT_PRESS) {
                pwrButtonPressed = true;
                Log::console(PSTR("PMU: PWR button pressed"));
            }
            if (irqs[2] & AXP2101_IRQ2_CHARGER_TIMER) {
                Log::console(PSTR("PMU WARNING: Charge safety timer expired - check battery pack config"));
            }
            if (irqs[2] & AXP2101_IRQ2_CHG_DONE) {
                Log::console(PSTR("PMU: Charge complete"));
            }
        } else if (AXPchip == 1) {
            uint8_t irqs[3];
            I2Cread(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS1, 3, irqs);
            uint8_t irq4 = I2CreadByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS4);
            if (irqs[0]) I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS1, irqs[0]);
            if (irqs[1]) I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS2, irqs[1]);
            if (irqs[2]) I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS3, irqs[2]);
            if (irq4) I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_IRQ_STATUS4, irq4);

            if (irqs[0] || irqs[1] || irqs[2] || irq4) {
                char irqDesc[64];
                decodeIRQs(1, irqs, irqDesc, sizeof(irqDesc));
                Log::console(PSTR("PMU IRQ: %02X,%02X,%02X,%02X [%s]"), irqs[0], irqs[1], irqs[2], irq4, irqDesc);
            }
            if (irqs[1] & AXP192_IRQ2_PEK_SHORT_PRESS) {
                pwrButtonPressed = true;
                Log::console(PSTR("PMU: PWR button pressed"));
            }
        }
    }

    // Periodic status report (every 5 minutes)
    unsigned long now = millis();
    if (now - lastPmuReport < 300000) return;
    lastPmuReport = now;

    float vbat = getBatteryVoltage();
    int pct = getBatteryPercentage();
    const char* stateStr = getChargeStateStr();
    float temp = getDieTemperature();
    bool vbus = isVbusPresent();

    if (AXPchip == 2) {
        uint8_t reg18 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
        uint8_t status1 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS1);
        uint8_t status2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS2);
        Log::console(PSTR("Power: %s (%s), %.2fV (%d%%), %.1fC [R18=%02X S1=%02X S2=%02X%s]"),
            vbus ? "USB/Sol" : "Battery", stateStr, vbat/1000.0, pct, temp,
            reg18, status1, status2, ((status2 & 0x08) && vbus) ? " VINDPM" : "");
    } else if (AXPchip == 1) {
        float chgCur = getBatteryChargeCurrent();
        float dischgCur = getBatteryDischargeCurrent();
        Log::console(PSTR("Power: %s (%s), %.2fV (%d%%), C:%dmA D:%dmA, %.1fC"),
            vbus ? "USB/Sol" : "Battery", stateStr, vbat/1000.0, pct,
            (int)chgCur, (int)dischgCur, temp);
    }
}

void Power::decodeIRQs(uint8_t chipType, const uint8_t* irqs, char* desc, size_t descLen) {
    desc[0] = '\0';
    if (chipType == 2) { // AXP2101
        if (irqs[0] & AXP2101_IRQ0_BAT_UNDER_TEMP_WORK) strlcat(desc, "BtL ", descLen);
        if (irqs[0] & AXP2101_IRQ0_BAT_OVER_TEMP_WORK)  strlcat(desc, "BtH ", descLen);
        if (irqs[0] & AXP2101_IRQ0_BAT_UNDER_TEMP_CHG)  strlcat(desc, "BcL ", descLen);
        if (irqs[0] & AXP2101_IRQ0_BAT_OVER_TEMP_CHG)   strlcat(desc, "BcH ", descLen);
        if (irqs[0] & AXP2101_IRQ0_GAUGE_NEW_SOC)       strlcat(desc, "SOC ", descLen);
        if (irqs[0] & AXP2101_IRQ0_WDT_TIMEOUT)         strlcat(desc, "WDT ", descLen);
        if (irqs[0] & AXP2101_IRQ0_SOC_WARN_LVL1)       strlcat(desc, "Lo1 ", descLen);
        if (irqs[0] & AXP2101_IRQ0_SOC_WARN_LVL2)       strlcat(desc, "Lo2 ", descLen);
        if (irqs[1] & AXP2101_IRQ1_PKEY_LONG_PRESS)     strlcat(desc, "PKL ", descLen);
        if (irqs[1] & AXP2101_IRQ1_PKEY_SHORT_PRESS)    strlcat(desc, "PKS ", descLen);
        if (irqs[1] & AXP2101_IRQ1_BAT_REMOVED)         strlcat(desc, "B- ", descLen);
        if (irqs[1] & AXP2101_IRQ1_BAT_INSERTED)        strlcat(desc, "B+ ", descLen);
        if (irqs[1] & AXP2101_IRQ1_VBUS_REMOVED)        strlcat(desc, "V- ", descLen);
        if (irqs[1] & AXP2101_IRQ1_VBUS_INSERTED)       strlcat(desc, "V+ ", descLen);
        if (irqs[2] & AXP2101_IRQ2_BAT_OVER_VOLTAGE)    strlcat(desc, "BOV ", descLen);
        if (irqs[2] & AXP2101_IRQ2_CHARGER_TIMER)       strlcat(desc, "CT! ", descLen);
        if (irqs[2] & AXP2101_IRQ2_DIE_OVER_TEMP)       strlcat(desc, "OT! ", descLen);
        if (irqs[2] & AXP2101_IRQ2_CHG_START)           strlcat(desc, "C+ ", descLen);
        if (irqs[2] & AXP2101_IRQ2_CHG_DONE)            strlcat(desc, "C= ", descLen);
        if (irqs[2] & AXP2101_IRQ2_BATFET_OVER_CUR)     strlcat(desc, "OC! ", descLen);
        if (irqs[2] & AXP2101_IRQ2_LDO_OVER_CUR)       strlcat(desc, "LOC ", descLen);
        if (irqs[2] & AXP2101_IRQ2_WDT_EXPIRE)          strlcat(desc, "WDE ", descLen);
    } else if (chipType == 1) { // AXP192
        if (irqs[0] & AXP192_IRQ0_VBUS_REMOVED)         strlcat(desc, "V- ", descLen);
        if (irqs[0] & AXP192_IRQ0_VBUS_INSERTED)        strlcat(desc, "V+ ", descLen);
        if (irqs[0] & AXP192_IRQ0_VBUS_OVER_VOLT)       strlcat(desc, "VOV ", descLen);
        if (irqs[1] & AXP192_IRQ1_BAT_UNDER_TEMP)       strlcat(desc, "BtL ", descLen);
        if (irqs[1] & AXP192_IRQ1_BAT_OVER_TEMP)        strlcat(desc, "BtH ", descLen);
        if (irqs[1] & AXP192_IRQ1_CHG_DONE)             strlcat(desc, "C= ", descLen);
        if (irqs[1] & AXP192_IRQ1_CHARGING)             strlcat(desc, "C+ ", descLen);
        if (irqs[1] & AXP192_IRQ1_BAT_REMOVED)          strlcat(desc, "B- ", descLen);
        if (irqs[1] & AXP192_IRQ1_BAT_INSERTED)         strlcat(desc, "B+ ", descLen);
        if (irqs[2] & AXP192_IRQ2_PEK_LONG_PRESS)       strlcat(desc, "PKL ", descLen);
        if (irqs[2] & AXP192_IRQ2_PEK_SHORT_PRESS)      strlcat(desc, "PKS ", descLen);
        if (irqs[2] & AXP192_IRQ2_CHIP_OVER_TEMP)       strlcat(desc, "OT! ", descLen);
    }
}
bool Power::wasPwrButtonPressed() {
    bool pressed = pwrButtonPressed;
    pwrButtonPressed = false;
    return pressed;
}


#else // Non-ESP32 (ESP8266) dummy implementations

Power::Power() : pmuWire(NULL) {}
void Power::checkAXP() {}
float Power::getBatteryVoltage() { return 0; }
int Power::getBatteryPercentage() { return 0; }
float Power::getVbusVoltage() { return 0; }
float Power::getVsysVoltage() { return 0; }
float Power::getVbusCurrent() { return 0; }
float Power::getBatteryChargeCurrent() { return 0; }
float Power::getBatteryDischargeCurrent() { return 0; }
float Power::getSystemCurrent() { return 0; }
bool Power::isVbusPresent() { return false; }
bool Power::isCharging() { return false; }
float Power::getBatteryCurrent() { return 0; }
float Power::getDieTemperature() { return 0; }
uint8_t Power::getChipType() { return 0; }
bool Power::wasPwrButtonPressed() { return false; }
const char* Power::getChargeStateStr() { return "No PMU"; }
void Power::checkPmuStatus() {}
void Power::getIRQStatus(uint8_t* irqs) { irqs[0] = irqs[1] = irqs[2] = 0; }
void Power::clearIRQ() {}
void Power::decodeIRQs(uint8_t chipType, const uint8_t* irqs, char* desc, size_t descLen) { desc[0] = '\0'; }
void Power::setGnssPower(bool on) {}
void Power::deepSleepSensors() {}
void Power::enableWatchdog() {}
void Power::disableWatchdog() {}
void Power::feedWatchdog() {}
void Power::setWatchdogSleepMode() {}
void Power::setWatchdogActiveMode() {}
void Power::I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data) {}
uint8_t Power::I2CreadByte(uint8_t Address, uint8_t Register) { return 0; }
void Power::I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data) {}

#endif
