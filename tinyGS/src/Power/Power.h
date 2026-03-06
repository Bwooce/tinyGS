/*
  Power.h - PMU power management for AXP192/AXP2101 chips

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

  Supported PMU chips:
    AXP192  - T-Beam V1.0, V1.1 (I2C addr 0x34, chip ID 0x03)
    AXP2101 - T-Beam V1.2, T-Beam Supreme (I2C addr 0x34, chip ID 0x4A)

  Datasheets:
    AXP2101 V1.0: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/AXP2101_Datasheet_V1.0_en.pdf
    AXP192:       http://images.shoutwiki.com/mindworks/8/8b/2020_infrasonic_wildfire_detector_APX192_Enhanced_Single_Cell_datasheet_en.pdf

  See also: https://github.com/lewisxhe/XPowersLib
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
// AXP192 Charge control registers
// REG 33H: Charge control 1
//   [7]   charge enable (1=on)
//   [6:5] target voltage: 00=4.1V, 01=4.15V, 10=4.2V, 11=4.36V
//   [4]   end current: 0=10%, 1=15%
//   [3:0] charge current: 0x3=360mA, 0x7=700mA, 0xB=1000mA
//         (0000=100,0001=190,0010=280,0011=360,...,1111=1320 mA)
#define AXP192_CHARGE1              0x33  // alias for AXP192_BAT_CHG_DIG_VOL
// REG 34H: Charge control 2
//   [7:6] precharge timeout: 00=30m, 01=40m, 10=50m, 11=60m
//   [5:3] external channel current (300-1000mA, 100mA/step)
//   [2]   external channel enable during charging
//   [1:0] CC timeout: 00=7h, 01=8h, 10=9h, 11=10h
//   Note: timeouts are always active (no enable bit unlike AXP2101)
#define AXP192_CHARGE2              0x34
#define AXP192_PEK_SET              0x36
#define AXP192_ADAPTER_OT_SET       0x39
#define AXP192_IRQ_STATUS1          0x44
#define AXP192_IRQ_STATUS2          0x45
#define AXP192_IRQ_STATUS3          0x46
#define AXP192_IRQ_STATUS4          0x47
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
#define AXP2101_VSYS_VOLT_H         0x3A
#define AXP2101_VSYS_VOLT_L         0x3B
#define AXP2101_DIE_TEMP_H          0x3C
#define AXP2101_DIE_TEMP_L          0x3D
// IRQ Enable registers
#define AXP2101_IRQ_ENABLE0         0x40
#define AXP2101_IRQ_ENABLE1         0x41
#define AXP2101_IRQ_ENABLE2         0x42
// IRQ Status registers (write-1-to-clear)
#define AXP2101_IRQ_STATUS0         0x48
#define AXP2101_IRQ_STATUS1         0x49
#define AXP2101_IRQ_STATUS2         0x4A
#define AXP2101_TS_PIN_CTRL         0x50
#define AXP2101_IPRECHG             0x61
#define AXP2101_ICC                 0x62
// AXP2101 Charge control registers (datasheet REG 62H, 67H)
// REG 62H: ICC charge current, bits[4:0]
//   N<=8: N*25 mA, N>8: 200+100*(N-8) mA.  0x0A=400mA, 0x0E=800mA, 0x10=1000mA
#define AXP2101_ICC_CHG_SET         0x62  // alias for AXP2101_ICC
#define AXP2101_ITERM               0x63
#define AXP2101_CV_VOLT             0x64
// REG 67H: Charge safety timer configuration
//   [7]   tmr_dt_en:   1=slow safety timer during DPM/thermal regulation
//   [6]   chg_tmr2_en: 1=enable CC charge timeout (timer2)
//   [5:4] chg_tmr2:    CC charge timeout: 00=5h, 01=8h, 10=12h, 11=20h
//   [3]   reserved
//   [2]   chg_tmr1_en: 1=enable precharge timeout (timer1)
//   [1:0] chg_tmr1:    precharge timeout: 00=40m, 01=50m, 10=60m, 11=70m
//   Default: 0xE6 (12h CC, 60m precharge, both timers enabled, DPM slowdown on)
//   Timeout expiry triggers battery safe mode (10mA trickle) and IRQ (irq2 bit 1)
#define AXP2101_CHG_TIMEOUT_CTRL    0x67
#define AXP2101_CHGLED_SET          0x69
#define AXP2101_BAT_CHG_BACKUP      0x6A
#define AXP2101_LDO_ONOFF_CTRL0     0x90
#define AXP2101_LDO_ONOFF_CTRL1     0x91
#define AXP2101_ALDO1_VOLT          0x92
#define AXP2101_ALDO2_VOLT          0x93
#define AXP2101_ALDO3_VOLT          0x94
#define AXP2101_ALDO4_VOLT          0x95
#define AXP2101_FUEL_GAUGE          0xA4
// Registers 0xA5/0xA6 are undocumented, not in XPowersLib -- removed

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
// Die temp formula: 22.0 + (7274 - raw) / 20.0  (per XPowersLib)
#define AXP2101_DIE_TEMP_BASE       22.0f
#define AXP2101_DIE_TEMP_CENTER     7274
#define AXP2101_DIE_TEMP_DIVISOR    20.0f

// AXP2101 IRQ bit definitions (registers 0x48, 0x49, 0x4A)
// Register 0x48 (irqs[0])
#define AXP2101_IRQ0_BAT_UNDER_TEMP_WORK   (1 << 0)
#define AXP2101_IRQ0_BAT_OVER_TEMP_WORK    (1 << 1)
#define AXP2101_IRQ0_BAT_UNDER_TEMP_CHG    (1 << 2)
#define AXP2101_IRQ0_BAT_OVER_TEMP_CHG     (1 << 3)
#define AXP2101_IRQ0_GAUGE_NEW_SOC         (1 << 4)
#define AXP2101_IRQ0_WDT_TIMEOUT           (1 << 5)
#define AXP2101_IRQ0_SOC_WARN_LVL1         (1 << 6)
#define AXP2101_IRQ0_SOC_WARN_LVL2         (1 << 7)
// Register 0x49 (irqs[1])
#define AXP2101_IRQ1_PKEY_POS_EDGE         (1 << 0)
#define AXP2101_IRQ1_PKEY_NEG_EDGE         (1 << 1)
#define AXP2101_IRQ1_PKEY_LONG_PRESS       (1 << 2)
#define AXP2101_IRQ1_PKEY_SHORT_PRESS      (1 << 3)
#define AXP2101_IRQ1_BAT_REMOVED           (1 << 4)
#define AXP2101_IRQ1_BAT_INSERTED          (1 << 5)
#define AXP2101_IRQ1_VBUS_REMOVED          (1 << 6)
#define AXP2101_IRQ1_VBUS_INSERTED         (1 << 7)
// Register 0x4A (irqs[2])
#define AXP2101_IRQ2_BAT_OVER_VOLTAGE      (1 << 0)
#define AXP2101_IRQ2_CHARGER_TIMER         (1 << 1)
#define AXP2101_IRQ2_DIE_OVER_TEMP         (1 << 2)
#define AXP2101_IRQ2_CHG_START             (1 << 3)
#define AXP2101_IRQ2_CHG_DONE              (1 << 4)
#define AXP2101_IRQ2_BATFET_OVER_CUR       (1 << 5)
#define AXP2101_IRQ2_LDO_OVER_CUR          (1 << 6)
#define AXP2101_IRQ2_WDT_EXPIRE            (1 << 7)

// AXP192 IRQ bit definitions (registers 0x44, 0x45, 0x46)
// Register 0x44 (irqs[0])
#define AXP192_IRQ0_ACIN_OVER_VOLT         (1 << 0)
#define AXP192_IRQ0_VBUS_BELOW_VHOLD       (1 << 1)
#define AXP192_IRQ0_VBUS_REMOVED           (1 << 2)
#define AXP192_IRQ0_VBUS_INSERTED          (1 << 3)
#define AXP192_IRQ0_VBUS_OVER_VOLT         (1 << 4)
#define AXP192_IRQ0_ACIN_REMOVED           (1 << 5)
#define AXP192_IRQ0_ACIN_INSERTED          (1 << 6)
#define AXP192_IRQ0_ACIN_OVER_VOLT2        (1 << 7)
// Register 0x45 (irqs[1])
#define AXP192_IRQ1_BAT_UNDER_TEMP         (1 << 0)
#define AXP192_IRQ1_BAT_OVER_TEMP          (1 << 1)
#define AXP192_IRQ1_CHG_DONE               (1 << 2)
#define AXP192_IRQ1_CHARGING               (1 << 3)
#define AXP192_IRQ1_BAT_EXIT_ACTIVATE      (1 << 4)
#define AXP192_IRQ1_BAT_ACTIVATE           (1 << 5)
#define AXP192_IRQ1_BAT_REMOVED            (1 << 6)
#define AXP192_IRQ1_BAT_INSERTED           (1 << 7)
// Register 0x46 (irqs[2])
#define AXP192_IRQ2_PEK_LONG_PRESS         (1 << 0)
#define AXP192_IRQ2_PEK_SHORT_PRESS        (1 << 1)
#define AXP192_IRQ2_CHIP_OVER_TEMP         (1 << 7)

struct PmuData {
    float battVol;
    float vbusVol;
    float vsysVol;       // AXP2101 only (regs 0x3A/0x3B)
    float battCur;       // Net current (AXP192 only, 0 on AXP2101)
    float battChgCur;    // Charge current (AXP192 only)
    float battDischgCur; // Discharge current (AXP192 only)
    float vbusCur;       // VBUS current (AXP192 only)
    float sysCur;        // System current (AXP192 only)
    float dieTemp;
    int battPct;
    bool vbusPresent;
    bool charging;
    uint8_t irqs[3];
    uint16_t raw_battVol;
    uint16_t raw_vbusVol;
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
     float getVsysVoltage();
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
     static void decodeIRQs(uint8_t chipType, const uint8_t* irqs, char* desc, size_t descLen);

     // PMU status check. Call from main loop.
     // Handles IRQ events immediately (interrupt-driven) and
     // periodic status reporting (voltage/percent/temp) every 5 minutes.
     void checkPmuStatus(bool force = false);

     // Return human-readable charge state string for display/web UI.
     // e.g. "Charging (CC)", "Done", "Not charging", "Pre-charge"
     const char* getChargeStateStr();

     void setGnssPower(bool on);
     bool wasPwrButtonPressed();
     int8_t getPmuIrqPin() { return pmuIrqPin; }
     void deepSleepSensors();
     TwoWire* getPmuWire() { return pmuWire; }
     Power();
private:
    void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data);
    uint8_t I2CreadByte(uint8_t Address, uint8_t Register);
    void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data);
    TwoWire* pmuWire;
    unsigned long lastPmuReport = 0;
    int8_t pmuIrqPin = -1;
    bool pwrButtonPressed = false;
    static void IRAM_ATTR pmuIrqHandler();
};
#endif
