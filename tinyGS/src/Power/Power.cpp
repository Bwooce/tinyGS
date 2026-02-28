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


byte AXPchip = 0;
byte pmustat1;
byte pmustat2;
byte pwronsta;
byte pwrofsta;
byte irqstat0;
byte irqstat1;
byte irqstat2;

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
  if (boardIdx == TTGO_TBEAM_SX1262) {
      Log::console(PSTR("PMU: Using Wire1 on pins %d/%d for Supreme"), SUPREME_PMU_SDA, SUPREME_PMU_SCL);
      Wire1.begin(SUPREME_PMU_SDA, SUPREME_PMU_SCL);
      pmuWire = &Wire1;
  } else
#endif
  {
      Wire.begin(board.OLED__SDA, board.OLED__SCL);                     // I2C_SDA, I2C_SCL on all new boards
      pmuWire = &Wire;
  }

  byte ChipID = I2CreadByte(0x34, 0x03);                            // read byte from xxx_IC_TYPE register
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  if (ChipID == XPOWERS_AXP192_CHIP_ID) { // 0x03
    AXPchip = 1;
    Log::console(PSTR("AXP192 found"));   // T-Beam V1.1 with AXP192 power controller
    I2CwriteByte(0x34, 0x28, 0xFF);       // Set LDO2 (LoRa) & LDO3 (GPS) to 3.3V , (1.8-3.3V, 100mV/step)
    regV = I2CreadByte(0x34, 0x12);       // Power Output Control
    regV = regV | 0x0C;                   // set bit 2 (LDO2) and bit 3 (LDO3)
    I2CwriteByte(0x34, 0x12, regV);       // and power channels now enabled
    I2CwriteByte(0x34, 0x84, 0b11000010); // Set ADC sample rate to 200hz, TS pin control 20uA
    I2CwriteByte(0x34, 0x82, 0xFF);       // |Set ADC to|
    I2CwriteByte(0x34, 0x83, 0x80);       // |All Enable|
    // Battery pack dependent charge config (reg 0x33 bit7=enable, bits[6:5]=4.2V target, bits[3:0]=current)
    {
      uint8_t bp = ConfigManager::getInstance().getBatteryPack();
      switch (bp) {
        case 2:
          I2CwriteByte(0x34, AXP192_CHARGE1, 0xC7); // 4.2V, 700mA
          I2CwriteByte(0x34, AXP192_CHARGE2, 0x83); // precharge 50m, CC timeout 10h
          Log::console(PSTR("PMU: Battery 2P config - 700mA charge, 10h/50m timeouts"));
          break;
        case 3:
          I2CwriteByte(0x34, AXP192_CHARGE1, 0xCB); // 4.2V, 1000mA
          I2CwriteByte(0x34, AXP192_CHARGE2, 0xC3); // precharge 60m, CC timeout 10h
          Log::console(PSTR("PMU: Battery 3P config - 1000mA charge, 10h/60m timeouts"));
          break;
        default: // 1P
          I2CwriteByte(0x34, AXP192_CHARGE1, 0xC3); // 4.2V, 360mA, 10% termination
          Log::console(PSTR("PMU: Battery 1P config - 360mA charge, default timeouts"));
          break;
      }
    }
    I2CwriteByte(0x34, 0x36, 0x0C);       // 128ms power on, 4s power off, 1s Long key press
    I2CwriteByte(0x34, 0x30, 0x80);       // Disable VBUS limits
    I2CwriteByte(0x34, 0x39, 0xFC);       // Set TS protection to 3.2256v -> disable
    I2CwriteByte(0x34, 0x32, 0x46);       // CHGLED controlled by the charging function
    pmustat1 = I2CreadByte(0x34, 0x00); pmustat2 = I2CreadByte(0x34, 0x01);
    irqstat0 = I2CreadByte(0x34, 0x44); irqstat1 = I2CreadByte(0x34, 0x45); irqstat2 = I2CreadByte(0x34, 0x46);
    Log::console(PSTR("PMU status1,status2 : %02X,%02X"), pmustat1, pmustat2);
    Log::console(PSTR("IRQ status 1,2,3    : %02X,%02X,%02X"), irqstat0, irqstat1, irqstat2);
  }
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  if (ChipID == XPOWERS_AXP2101_CHIP_ID) {// 0x4A
    AXPchip = 2;
    Log::console(PSTR("AXP2101 found"));  // T-Beam V1.2 or Supreme with AXP2101 power controller

#if CONFIG_IDF_TARGET_ESP32S3
    if (boardIdx == TTGO_TBEAM_SX1262) {
      // T-Beam Supreme: ALDO1 (display/sensors), ALDO3 (radio), ALDO4 (GPS)
      Log::console(PSTR("Configuring AXP2101 for T-Beam Supreme"));
      I2CwriteByte(0x34, 0x92, 0x1C);       // set ALDO1 voltage to 3.3V (Display + sensors)
      I2CwriteByte(0x34, 0x94, 0x1C);       // set ALDO3 voltage to 3.3V (Radio)
      I2CwriteByte(0x34, 0x95, 0x1C);       // set ALDO4 voltage to 3.3V (GPS)
      I2CwriteByte(0x34, 0x6A, 0x04);       // set Button battery voltage to 3.0V
      I2CwriteByte(0x34, 0x64, 0x03);       // set Main battery voltage to 4.2V
      I2CwriteByte(0x34, 0x63, 0x15);       // set Main battery term charge current to 125mA

      // Battery pack dependent charge config
      {
        uint8_t bp = ConfigManager::getInstance().getBatteryPack();
        switch (bp) {
          case 2:
            I2CwriteByte(0x34, 0x61, 0x06);               // precharge 150mA
            I2CwriteByte(0x34, AXP2101_ICC_CHG_SET, 0x0E); // charge current 800mA
            I2CwriteByte(0x34, AXP2101_CHG_TIMEOUT_CTRL, 0xF6); // slow_DPM, 20h CC, 60m pre
            Log::console(PSTR("PMU: Battery 2P config - 800mA charge, 20h/60m timeouts"));
            break;
          case 3:
            I2CwriteByte(0x34, 0x61, 0x07);               // precharge 175mA
            I2CwriteByte(0x34, AXP2101_ICC_CHG_SET, 0x10); // charge current 1000mA
            I2CwriteByte(0x34, AXP2101_CHG_TIMEOUT_CTRL, 0xF7); // slow_DPM, 20h CC, 70m pre
            Log::console(PSTR("PMU: Battery 3P config - 1000mA charge, 20h/70m timeouts"));
            break;
          default: // 1P
            I2CwriteByte(0x34, 0x61, 0x05);               // precharge 125mA
            I2CwriteByte(0x34, AXP2101_ICC_CHG_SET, 0x0A); // charge current 400mA
            I2CwriteByte(0x34, AXP2101_CHG_TIMEOUT_CTRL, 0xE5); // slow_DPM, 12h CC, 50m pre
            Log::console(PSTR("PMU: Battery 1P config - 400mA charge, 12h/50m timeouts"));
            break;
        }
      }

      // Disable unused rails to minimize quiescent current
      I2CwriteByte(0x34, 0x91, 0x00);       // Disable BLDO1, BLDO2, DLDO1, DLDO2

      regV = I2CreadByte(0x34, AXP2101_LDO_ONOFF_CTRL0);
      regV &= ~(1 << AXP2101_ALDO2_BIT);   // Disable ALDO2 (unused on Supreme)
      regV = regV | (1 << AXP2101_ALDO1_BIT) | (1 << AXP2101_ALDO3_BIT) | (1 << AXP2101_ALDO4_BIT);
      I2CwriteByte(0x34, AXP2101_LDO_ONOFF_CTRL0, regV);
    } else
#endif
    {
      // T-Beam V1.2: ALDO2 (radio), ALDO3 (GPS)
      I2CwriteByte(0x34, 0x93, 0x1C);       // set ALDO2 voltage to 3.3V (LoRa VCC)
      I2CwriteByte(0x34, 0x94, 0x1C);       // set ALDO3 voltage to 3.3V (GPS VDD)
      I2CwriteByte(0x34, 0x6A, 0x04);       // set Button battery voltage to 3.0V
      I2CwriteByte(0x34, 0x64, 0x03);       // set Main battery voltage to 4.2V
      I2CwriteByte(0x34, 0x63, 0x15);       // set Main battery term charge current to 125mA

      // Battery pack dependent charge config
      {
        uint8_t bp = ConfigManager::getInstance().getBatteryPack();
        switch (bp) {
          case 2:
            I2CwriteByte(0x34, 0x61, 0x06);               // precharge 150mA
            I2CwriteByte(0x34, AXP2101_ICC_CHG_SET, 0x0E); // charge current 800mA
            I2CwriteByte(0x34, AXP2101_CHG_TIMEOUT_CTRL, 0xF6); // slow_DPM, 20h CC, 60m pre
            Log::console(PSTR("PMU: Battery 2P config - 800mA charge, 20h/60m timeouts"));
            break;
          case 3:
            I2CwriteByte(0x34, 0x61, 0x07);               // precharge 175mA
            I2CwriteByte(0x34, AXP2101_ICC_CHG_SET, 0x10); // charge current 1000mA
            I2CwriteByte(0x34, AXP2101_CHG_TIMEOUT_CTRL, 0xF7); // slow_DPM, 20h CC, 70m pre
            Log::console(PSTR("PMU: Battery 3P config - 1000mA charge, 20h/70m timeouts"));
            break;
          default: // 1P
            I2CwriteByte(0x34, 0x61, 0x05);               // precharge 125mA
            I2CwriteByte(0x34, AXP2101_ICC_CHG_SET, 0x0A); // charge current 400mA
            I2CwriteByte(0x34, AXP2101_CHG_TIMEOUT_CTRL, 0xE5); // slow_DPM, 12h CC, 50m pre
            Log::console(PSTR("PMU: Battery 1P config - 400mA charge, 12h/50m timeouts"));
            break;
        }
      }
      regV = I2CreadByte(0x34, AXP2101_LDO_ONOFF_CTRL0);
      regV = regV | (1 << AXP2101_ALDO2_BIT) | (1 << AXP2101_ALDO3_BIT);
      I2CwriteByte(0x34, AXP2101_LDO_ONOFF_CTRL0, regV);
    }

    regV = I2CreadByte(0x34, 0x18);       // XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL
    regV = regV | 0x06;                   // set bit 1 (Main Battery) and bit 2 (Button battery)
    I2CwriteByte(0x34, 0x18, regV);       // and chargers now enabled
    I2CwriteByte(0x34, 0x14, 0x30);       // set minimum system voltage to 4.4V (default 4.7V), for poor USB cables
    I2CwriteByte(0x34, 0x15, 0x05);       // set VBUS input current limit to 2000mA
    I2CwriteByte(0x34, 0x24, 0x06);       // set Vsys for PWROFF threshold to 3.2V (default - 2.6V and kill battery)
    I2CwriteByte(0x34, 0x50, 0x14);       // set TS pin to EXTERNAL input (not temperature)
    I2CwriteByte(0x34, 0x69, 0x01);       // set CHGLED for 'type A' and enable pin function
    I2CwriteByte(0x34, 0x27, 0x00);       // set IRQLevel/OFFLevel/ONLevel to minimum (1S/4S/128mS)
    I2CwriteByte(0x34, 0x30, 0xFF);       // enable all ADC channels
    pmustat1 = I2CreadByte(0x34, 0x00); pmustat2 = I2CreadByte(0x34, 0x01);
    pwronsta = I2CreadByte(0x34, 0x20); pwrofsta = I2CreadByte(0x34, 0x21);
    irqstat0 = I2CreadByte(0x34, 0x48); irqstat1 = I2CreadByte(0x34, 0x49); irqstat2 = I2CreadByte(0x34, 0x4A);
    Log::console(PSTR("PMU status1,status2 : %02X,%02X"), pmustat1, pmustat2);
    Log::console(PSTR("PWRON,PWROFF status : %02X,%02X"), pwronsta, pwrofsta);
    Log::console(PSTR("IRQ status 0,1,2    : %02X,%02X,%02X"), irqstat0, irqstat1, irqstat2);

#if CONFIG_IDF_TARGET_ESP32S3
    if (boardIdx == TTGO_TBEAM_SX1262) {
        deepSleepSensors();
    }
#endif
  }
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
        Log::console(PSTR("Found QMC5883L at 0x0D, putting to sleep"));
        Wire.beginTransmission(0x0D);
        Wire.write(0x09); // Control Register 1
        Wire.write(0x00); // Standby Mode
        Wire.endTransmission();
    }

    // Check for BME280 (Environment) at 0x77
    Wire.beginTransmission(0x77);
    if (Wire.endTransmission() == 0) {
        Log::console(PSTR("Found BME280 at 0x77, putting to sleep"));
        Wire.beginTransmission(0x77);
        Wire.write(0xF4); // CTRL_MEAS register
        Wire.write(0x00); // Sleep mode
        Wire.endTransmission();
    }
}

float Power::getBatteryVoltage() {
    uint8_t boardIdx = ConfigManager::getInstance().getBoard();
    if (AXPchip == 1) {
        // AXP192: 12-bit ADC, 1.1mV/step
        uint8_t h8 = I2CreadByte(0x34, 0x78);
        uint8_t l4 = I2CreadByte(0x34, 0x79);
        return ((h8 << 4) | (l4 & 0x0F)) * 1.1f / 1000.0f;
    } else if (AXPchip == 2) {
        // AXP2101: 14-bit register pair, 1mV/step
        uint8_t h = I2CreadByte(0x34, AXP2101_BATTERY_VOLT_H);
        uint8_t l = I2CreadByte(0x34, AXP2101_BATTERY_VOLT_L);
        uint16_t raw = ((uint16_t)(h & AXP2101_BATT_VOLT_MASK) << AXP2101_BATT_VOLT_SHIFT) | l;
        return raw / 1000.0f;
    } else {
        // Fallback: direct ADC (e.g. Heltec boards)
        return analogRead(36) * 2.0f / 4095.0f * 3.3f;
    }
}

int Power::getBatteryPercentage() {
    if (AXPchip == 2) {
        return I2CreadByte(0x34, AXP2101_FUEL_GAUGE) & 0x7F;
    }
    // Simple estimation from voltage for AXP192 and others
    float v = getBatteryVoltage();
    if (v > 4.2f) return 100;
    if (v < 3.0f) return 0;
    return (int)((v - 3.0f) / 1.2f * 100.0f);
}

float Power::getVbusVoltage() {
    if (AXPchip == 1) {
        uint8_t h8 = I2CreadByte(0x34, 0x5A);
        uint8_t l4 = I2CreadByte(0x34, 0x5B);
        return ((h8 << 4) | (l4 & 0x0F)) * 1.7f / 1000.0f;
    } else if (AXPchip == 2) {
        uint8_t h = I2CreadByte(0x34, AXP2101_VBUS_VOLT_H);
        uint8_t l = I2CreadByte(0x34, AXP2101_VBUS_VOLT_L);
        uint16_t raw = ((uint16_t)(h & AXP2101_VBUS_VOLT_MASK) << AXP2101_VBUS_VOLT_SHIFT) | l;
        return raw / 1000.0f;
    }
    return 0.0f;
}

void Power::setGnssPower(bool on) {
    uint8_t boardIdx = ConfigManager::getInstance().getBoard();
    if (AXPchip != 2) return;

    byte regV = I2CreadByte(0x34, AXP2101_LDO_ONOFF_CTRL0);

#if CONFIG_IDF_TARGET_ESP32S3
    if (boardIdx == TTGO_TBEAM_SX1262) {
        // Supreme: GPS on ALDO4
        if (on) regV |= (1 << AXP2101_ALDO4_BIT);
        else    regV &= ~(1 << AXP2101_ALDO4_BIT);
    } else
#endif
    {
        // T-Beam V1.2: GPS on ALDO3
        if (on) regV |= (1 << AXP2101_ALDO3_BIT);
        else    regV &= ~(1 << AXP2101_ALDO3_BIT);
    }

    I2CwriteByte(0x34, AXP2101_LDO_ONOFF_CTRL0, regV);
    Log::console(PSTR("GNSS power %s"), on ? "ON" : "OFF");
}

uint8_t Power::getAXPchip() {
    return AXPchip;
}

const char* Power::getChargeStateStr() {
    if (AXPchip == 2) {
        uint8_t status2 = I2CreadByte(0x34, AXP2101_STATUS2);
        uint8_t chgState = status2 & 0x07;
        switch (chgState) {
          case 0: return "Idle";
          case 1: return "Pre-charge";
          case 2: return "CC charging";
          case 3: return "CV charging";
          case 4: return "Done";
          default: return "Unknown";
        }
    } else if (AXPchip == 1) {
        uint8_t status1 = I2CreadByte(0x34, AXP192_MODE_CHGSTATUS);
        if (status1 & 0x40) return "Charging";
        return "Not charging";
    }
    return "No PMU";
}

void Power::checkPmuStatus() {
    if (AXPchip == 0) return;

    unsigned long now = millis();
    if (now - lastPmuCheck < 60000) return;
    lastPmuCheck = now;

    float vbat = getBatteryVoltage();
    int pct = getBatteryPercentage();

    if (AXPchip == 2) {
        // AXP2101 charge state from STATUS2[2:0]
        const char* stateStr = getChargeStateStr();
        Log::debug(PSTR("PMU: %s, Vbat=%.2fV (%d%%)"), stateStr, vbat, pct);

        // Read and clear IRQ status (write-1-to-clear)
        uint8_t irq0 = I2CreadByte(0x34, AXP2101_IRQ_STATUS0);
        uint8_t irq1 = I2CreadByte(0x34, AXP2101_IRQ_STATUS1);
        uint8_t irq2 = I2CreadByte(0x34, AXP2101_IRQ_STATUS2);
        if (irq0) I2CwriteByte(0x34, AXP2101_IRQ_STATUS0, irq0);
        if (irq1) I2CwriteByte(0x34, AXP2101_IRQ_STATUS1, irq1);
        if (irq2) I2CwriteByte(0x34, AXP2101_IRQ_STATUS2, irq2);

        if (irq0 || irq1 || irq2) {
            Log::console(PSTR("PMU IRQ: %02X,%02X,%02X"), irq0, irq1, irq2);
        }
        // IRQ2 (0x4A) bit 1 = charge safety timer expired
        if (irq2 & 0x02) {
            Log::console(PSTR("PMU WARNING: Charge safety timer expired - check battery pack config"));
        }
        // IRQ2 (0x4A) bit 4 = charge done
        if (irq2 & 0x10) {
            Log::console(PSTR("PMU: Charge complete"));
        }
    } else if (AXPchip == 1) {
        // AXP192 charge state from reg 0x01
        uint8_t chgStatus = I2CreadByte(0x34, AXP192_MODE_CHGSTATUS);
        const char* stateStr = (chgStatus & 0x40) ? "Charging" : "Not charging";
        Log::debug(PSTR("PMU: %s, Vbat=%.2fV (%d%%)"), stateStr, vbat, pct);

        // Read and clear IRQ status
        uint8_t irq1 = I2CreadByte(0x34, AXP192_IRQ_STATUS1);
        uint8_t irq2 = I2CreadByte(0x34, AXP192_IRQ_STATUS2);
        uint8_t irq3 = I2CreadByte(0x34, AXP192_IRQ_STATUS3);
        uint8_t irq4 = I2CreadByte(0x34, AXP192_IRQ_STATUS4);
        if (irq1) I2CwriteByte(0x34, AXP192_IRQ_STATUS1, irq1);
        if (irq2) I2CwriteByte(0x34, AXP192_IRQ_STATUS2, irq2);
        if (irq3) I2CwriteByte(0x34, AXP192_IRQ_STATUS3, irq3);
        if (irq4) I2CwriteByte(0x34, AXP192_IRQ_STATUS4, irq4);

        if (irq1 || irq2 || irq3 || irq4) {
            Log::console(PSTR("PMU IRQ: %02X,%02X,%02X,%02X"), irq1, irq2, irq3, irq4);
        }
    }
}
