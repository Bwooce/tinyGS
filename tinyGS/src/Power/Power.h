/*
  Power.h - Class responsible of controlling the display
  
  Copyright (C) 2020 -2024 Megazaic39 [E16] , @G4lile0, @gmag12 and @dev_4m1g0

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

#ifndef Power_h
#define Power_h

#include <Arduino.h>
#include <Wire.h>
#include "../ConfigManager/ConfigManager.h"
#include "../Status.h"

#if defined(ESP32)

#define XPOWERS_AXP192_CHIP_ID      0x03
#define XPOWERS_AXP2101_CHIP_ID     0x4A

// AXP192/2101 Common Slave Address
#define AXP_SLAVE_ADDRESS           0x34

// AXP192 Specific Registers
#define AXP192_SLAVE_ADDRESS        0x34
#define AXP192_STATUS               0x00
#define AXP192_MODE_CHGSTATUS       0x01
#define AXP192_CHIP_ID              0x03
#define AXP192_LDO23_OUT_VOL        0x28
#define AXP192_VBUS_VOL_LIMIT       0x30
#define AXP192_CHGLED_CONTROL       0x32
#define AXP192_BAT_CHG_DIG_VOL      0x33
#define AXP192_PEK_SET              0x36
#define AXP192_ADAPTER_OT_SET       0x39
#define AXP192_IRQ_STATUS1          0x44
#define AXP192_IRQ_STATUS2          0x45
#define AXP192_IRQ_STATUS3          0x46
#define AXP192_VBUS_VOL_H           0x5A
#define AXP192_VBUS_VOL_L           0x5B
#define AXP192_VBUS_CUR_H           0x5C
#define AXP192_VBUS_CUR_L           0x5D
#define AXP192_DIE_TEMP_H           0x5E
#define AXP192_DIE_TEMP_L           0x5F
#define AXP192_BAT_AVERVOL_H        0x78
#define AXP192_BAT_AVERVOL_L        0x79
#define AXP192_BAT_AVERCHGCUR_H     0x7A
#define AXP192_BAT_AVERCHGCUR_L     0x7B
#define AXP192_BAT_AVERDISCHGCUR_H  0x7C
#define AXP192_BAT_AVERDISCHGCUR_L  0x7D
#define AXP192_ADC_EN1              0x82
#define AXP192_ADC_EN2              0x83
#define AXP192_ADC_SPEED            0x84

// AXP192 Bitmasks and Shifts
#define AXP192_ADC_MSB_SHIFT        4
#define AXP192_ADC_LSB_MASK         0x0F
#define AXP192_VBUS_VOL_STEP        1.7f
#define AXP192_VBUS_CUR_STEP        0.375f
#define AXP192_BAT_VOL_STEP         1.1f
#define AXP192_CUR_MSB_SHIFT        5
#define AXP192_CUR_LSB_MASK         0x1F
#define AXP192_CUR_STEP             0.5f
#define AXP192_DIE_TEMP_STEP        0.1f
#define AXP192_DIE_TEMP_OFFSET      -144.7f

// AXP2101 Specific Registers
#define AXP2101_SLAVE_ADDRESS       0x34
#define AXP2101_STATUS1             0x00
#define AXP2101_STATUS2             0x01
#define AXP2101_IC_TYPE             0x03
#define AXP2101_DATA_BUFFER0        0x04
#define AXP2101_COMMON_CONFIG       0x10
#define AXP2101_BAT_V_LIMIT         0x14
#define AXP2101_VBUS_V_LIMIT        0x15
#define AXP2101_CHG_GAUGE_WDT_CTRL  0x18
#define AXP2101_PWRON_STATUS        0x20
#define AXP2101_PWROFF_STATUS       0x21
#define AXP2101_VOFF_SET            0x24
#define AXP2101_PWROK_SET           0x25
#define AXP2101_PWROK_DLY           0x26
#define AXP2101_PEK_SET             0x27
#define AXP2101_ADC_CONFIG          0x30
#define AXP2101_BATTERY_VOLT_H      0x34
#define AXP2101_BATTERY_VOLT_L      0x35
#define AXP2101_VBUS_VOLT_H         0x38
#define AXP2101_VBUS_VOLT_L         0x39
#define AXP2101_VBUS_CUR_H          0x3A
#define AXP2101_VBUS_CUR_L          0x3B
#define AXP2101_DIE_TEMP_H          0x3C
#define AXP2101_DIE_TEMP_L          0x3D
#define AXP2101_BATT_CHG_CUR_H      0x40
#define AXP2101_BATT_CHG_CUR_L      0x41
#define AXP2101_BATT_DISCHG_CUR_H   0x42
#define AXP2101_BATT_DISCHG_CUR_L   0x43
#define AXP2101_IRQ_STATUS0         0x48
#define AXP2101_IRQ_STATUS1         0x49
#define AXP2101_IRQ_STATUS2         0x4A
#define AXP2101_TS_PIN_CTRL         0x50
#define AXP2101_IPRECHG             0x61
#define AXP2101_ICC                 0x62
#define AXP2101_ITERM               0x63
#define AXP2101_CV_VOLT             0x64
#define AXP2101_CHGLED_SET          0x69
#define AXP2101_BAT_CHG_BACKUP      0x6A
#define AXP2101_LDO_ONOFF_CTRL0     0x90
#define AXP2101_LDO_ONOFF_CTRL1     0x91
#define AXP2101_ALDO1_VOLT          0x92
#define AXP2101_ALDO2_VOLT          0x93
#define AXP2101_ALDO3_VOLT          0x94
#define AXP2101_ALDO4_VOLT          0x95
#define AXP2101_FUEL_GAUGE          0xA4
#define AXP2101_BATT_CUR_H          0xA5
#define AXP2101_BATT_CUR_L          0xA6

// AXP2101 Bitmasks and Shifts
#define AXP2101_ALDO1_BIT           0
#define AXP2101_ALDO2_BIT           1
#define AXP2101_ALDO3_BIT           2
#define AXP2101_ALDO4_BIT           3
#define AXP2101_VBUS_PRESENT_BIT    5
#define AXP2101_CHG_STATUS_MASK     0x07
#define AXP2101_ADC_MSB_MASK        0x3F
#define AXP2101_VOLT_MSB_SHIFT      8
#define AXP2101_CUR_MSB_SHIFT       8
#define AXP2101_DIE_TEMP_MSB_MASK   0x3F
#define AXP2101_DIE_TEMP_STEP       0.1f
#define AXP2101_DIE_TEMP_OFFSET     -644.7f

struct PmuData {
    float battVol;
    float vbusVol;
    float battCur; // Net Current
    float battChgCur; // Indiv Charge
    float battDischgCur; // Indiv Discharge
    float vbusCur;
    float sysCur;
    float dieTemp;
    int battPct;
    bool vbusPresent;
    bool charging;
    uint8_t irqs[3];
    uint16_t raw_battVol;
    uint16_t raw_vbusVol;
    uint16_t raw_battCur;
    uint16_t raw_vbusCur;
    uint16_t raw_dieTemp;
};

#endif // ESP32

extern Status status;

class Power {
public:
    static Power& getInstance()
    {
        static Power instance; 
        return instance;
    }
     void checkAXP(); 
     float getBatteryVoltage();
     int getBatteryPercentage();
     float getVbusVoltage();
     float getVbusCurrent();
     float getSystemCurrent();
     bool isVbusPresent();
     bool isCharging();
     float getBatteryCurrent();
     float getBatteryChargeCurrent();
     float getBatteryDischargeCurrent();
     float getDieTemperature();
     uint8_t getChipType(); 
#if defined(ESP32)
     void getPmuData(PmuData* data);
#endif
     void getIRQStatus(uint8_t* irqs);
     void clearIRQ();
     void setGnssPower(bool on);
     void deepSleepSensors();
     TwoWire* getPmuWire() { return pmuWire; }
     Power();
private:
    void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data);
    uint8_t I2CreadByte(uint8_t Address, uint8_t Register);
    void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data);
    TwoWire* pmuWire;
};
#endif
