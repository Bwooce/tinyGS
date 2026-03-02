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

#define LEGACY_BATT_PIN 36

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
  
  if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
      Log::console(PSTR("PMU: Using Supreme Wire1 on pins %d/%d"), SUPREME_PMU_SDA, SUPREME_PMU_SCL);
      Wire1.begin(SUPREME_PMU_SDA, SUPREME_PMU_SCL);
      pmuWire = &Wire1;
  } else {
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
    I2CwriteByte(AXP192_SLAVE_ADDRESS, AXP192_BAT_CHG_DIG_VOL, 0xC3);       // Bat charge voltage to 4.2, Current 360MA and 10% for stop charging
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
    Log::console(PSTR("IRQ status 1,2,3    : %02X,%02X,%02X"), irqstat0, irqstat1, irqstat2);
  }
  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  if (ChipID == XPOWERS_AXP2101_CHIP_ID) {// 0x4A
    AXPchip = 2;
    Log::console(PSTR("AXP2101 found"));  // T-Beam V1.2 or Supreme with AXP2101 power controller
    
    if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
      // T-Beam Supreme
      Log::console(PSTR("Configuring for T-Beam Supreme"));
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO1_VOLT, 0x1C);       // set ALDO1 voltage to 3.3V ( Display )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO3_VOLT, 0x1C);       // set ALDO3 voltage to 3.3V ( Radio )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO4_VOLT, 0x1C);       // set ALDO4 voltage to 3.3V ( GPS )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_BAT_CHG_BACKUP, 0x04);       // set Button battery voltage to 3.0V ( backup battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CV_VOLT, 0x03);       // set Main battery voltage to 4.2V ( 18650 battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x05);       // set Main battery precharge current to 125mA
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC, 0x0A);       // set Main battery charger current to 400mA
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ITERM, 0x15);       // set Main battery term charge current to 125mA
      
      // Explicitly disable unused rails (ALDO2, BLDO1/2, DLDO1/2)
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL1, 0x00); // Disable BLDO1, BLDO2, DLDO1, DLDO2

      regV = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0);
      regV &= ~(1 << AXP2101_ALDO2_BIT); // Disable ALDO2
      regV = regV | (1 << AXP2101_ALDO1_BIT) | (1 << AXP2101_ALDO3_BIT) | (1 << AXP2101_ALDO4_BIT);
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0, regV);       // and power channels now enabled
    } else {
      // T-Beam V1.2 (SDA likely 21)
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO2_VOLT, 0x1C);       // set ALDO2 voltage to 3.3V ( LoRa VCC )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ALDO3_VOLT, 0x1C);       // set ALDO3 voltage to 3.3V ( GPS VDD )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_BAT_CHG_BACKUP, 0x04);       // set Button battery voltage to 3.0V ( backup battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CV_VOLT, 0x03);       // set Main battery voltage to 4.2V ( 18650 battery )
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_IPRECHG, 0x05);       // set Main battery precharge current to 125mA
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ICC, 0x0A);       // set Main battery charger current to 400mA
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ITERM, 0x15);       // set Main battery term charge current to 125mA
      regV = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0);
      regV = regV | (1 << AXP2101_ALDO2_BIT) | (1 << AXP2101_ALDO3_BIT);
      I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_LDO_ONOFF_CTRL0, regV);       // and power channels now enabled
    }

    regV = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL);
    regV = regV | 0x06;                   // set bit 1 (Main Battery) and bit 2 (Button battery)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHG_GAUGE_WDT_CTRL, regV);       // and chargers now enabled
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_BAT_V_LIMIT, 0x30);       // set minimum system voltage to 4.4V (default 4.7V)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_VBUS_V_LIMIT, 0x05);       // set input voltage limit to 4.28v
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_VOFF_SET, 0x06);       // set Vsys for PWROFF threshold to 3.2V
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_TS_PIN_CTRL, 0x14);       // set TS pin to EXTERNAL input (not temperature)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_CHGLED_SET, 0x01);       // set CHGLED for 'type A' and enable pin function
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_PEK_SET, 0x00);       // set IRQLevel/OFFLevel/ONLevel to minimum (1S/4S/128mS)
    I2CwriteByte(AXP2101_SLAVE_ADDRESS, AXP2101_ADC_CONFIG, 0xFF);       // enable ADC for SYS, VBUS, TS and Battery
    pmustat1 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS1); 
    pmustat2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_STATUS2);
    pwronsta = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_PWRON_STATUS); 
    pwrofsta = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_PWROFF_STATUS);
    irqstat0 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS0); 
    irqstat1 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS1); 
    irqstat2 = I2CreadByte(AXP2101_SLAVE_ADDRESS, AXP2101_IRQ_STATUS2);
    Log::console(PSTR("PMU status1,status2 : %02X,%02X"), pmustat1, pmustat2);
    Log::console(PSTR("PWRON,PWROFF status : %02X,%02X"), pwronsta, pwrofsta);
    Log::console(PSTR("IRQ status 0,1,2    : %02X,%02X,%02X"), irqstat0, irqstat1, irqstat2);
    
    if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
        deepSleepSensors();
    }
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
        voltage = ((buf[0] << AXP192_BAT_VOL_MSB_SHIFT) | (buf[1] & AXP192_BAT_VOL_LSB_MASK)) * AXP192_BAT_VOL_STEP;
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
        voltage = ((buf[0] << AXP192_VBUS_VOL_MSB_SHIFT) | (buf[1] & AXP192_VBUS_VOL_LSB_MASK)) * AXP192_VBUS_VOL_STEP;
    } else if (AXPchip == 2) { // AXP2101
        uint8_t buf[2] = {0, 0};
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_VBUS_VOLT_H, 2, buf);
        // AXP2101 VBUS: 16-bit (H << 8 | L), 1mV/bit
        voltage = (float)((buf[0] << AXP2101_VOLT_MSB_SHIFT) | buf[1]);
    }
    return voltage;
}

float Power::getDieTemperature() {
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_DIE_TEMP_H, 2, buf);
        uint16_t raw = (buf[0] << 4) | (buf[1] & 0x0F);
        return raw * 0.1 - 144.7;
    } else if (AXPchip == 2) { // AXP2101
        uint8_t buf[2];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_DIE_TEMP_H, 2, buf);
        // AXP2101: 14-bit, 0.1 degC / bit. Formula: (MSB << 8 | LSB) * 0.1 - 144.7
        uint16_t raw = ((buf[0] & AXP2101_DIE_TEMP_MSB_MASK) << 8) | buf[1];
        return raw * 0.1 - 144.7;
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
        
        if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) { // Supreme
             if (on) reg |= (1 << AXP2101_ALDO4_BIT); 
             else reg &= ~(1 << AXP2101_ALDO4_BIT);
        } else {
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
    if (AXPchip == 1) { // AXP192
        uint8_t buf[2];
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_BAT_AVERCHGCUR_H, 2, buf);
        float charge = ((buf[0] << AXP192_BAT_CUR_MSB_SHIFT) | (buf[1] & AXP192_BAT_CUR_LSB_MASK)) * AXP192_BAT_CUR_STEP;
        I2Cread(AXP192_SLAVE_ADDRESS, AXP192_BAT_AVERDISCHGCUR_H, 2, buf);
        float discharge = ((buf[0] << AXP192_BAT_CUR_MSB_SHIFT) | (buf[1] & AXP192_BAT_CUR_LSB_MASK)) * AXP192_BAT_CUR_STEP;
        return charge - discharge;
    } else if (AXPchip == 2) { // AXP2101
        uint8_t buf[2];
        // Read 16-bit signed current from E-Gauge registers 0xA5/0xA6
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_BATT_CUR_H, 2, buf);
        int16_t cur = (int16_t)((buf[0] << AXP2101_CUR_MSB_SHIFT) | buf[1]);
        return (float)cur;
    }
    return 0;
}

void Power::getPmuData(PmuData* data) {
    data->battVol = getBatteryVoltage();
    data->vbusVol = getVbusVoltage();
    data->battCur = getBatteryCurrent();
    data->dieTemp = getDieTemperature();
    data->battPct = getBatteryPercentage();
    data->vbusPresent = isVbusPresent();
    data->charging = isCharging();
    getIRQStatus(data->irqs);
    
    if (AXPchip == 2) { // AXP2101
        uint8_t buf[2];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_BATTERY_VOLT_H, 2, buf);
        data->raw_battVol = (buf[0] << 8) | buf[1];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_VBUS_VOLT_H, 2, buf);
        data->raw_vbusVol = (buf[0] << 8) | buf[1];
        I2Cread(AXP2101_SLAVE_ADDRESS, AXP2101_BATT_CUR_H, 2, buf);
        data->raw_battCur = (buf[0] << 8) | buf[1];
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
