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


#ifndef POWER_H
#define POWER_H
#include "Arduino.h"
#include <Wire.h>
#include "../Status.h"
#include "../ConfigManager/ConfigManager.h"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
// * * * * * * * A X P   C H I P   C O N F I G * * * * * * * * * *
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

// Chip detection constants (from XPowersLib REG/*Constants.h).
// Read register 0x03 (IC_TYPE) at the slave address; the returned
// byte is the chip ID.  Only AXP192 and AXP2101 are used by TinyGS boards.
#define AXP192_SLAVE_ADDRESS    (0x34)
#define XPOWERS_AXP192_IC_TYPE  (0x03) // register address
#define XPOWERS_AXP192_CHIP_ID  (0x03) // expected value

#define AXP2101_SLAVE_ADDRESS   (0x34)
#define XPOWERS_AXP2101_IC_TYPE (0x03)
#define XPOWERS_AXP2101_CHIP_ID (0x4A)

// AXP2101 LDO control (datasheet REG 90H)
#define AXP2101_LDO_ONOFF_CTRL0     0x90
#define AXP2101_ALDO1_BIT           0
#define AXP2101_ALDO2_BIT           1
#define AXP2101_ALDO3_BIT           2
#define AXP2101_ALDO4_BIT           3

// AXP2101 ADC data registers (datasheet REG 34H-39H)
// Battery voltage: (VOLT_H << 8 | VOLT_L) * 1.1mV / 1000
#define AXP2101_BATTERY_VOLT_H      0x34
#define AXP2101_BATTERY_VOLT_L      0x35
// VBUS voltage: (VOLT_H << 8 | VOLT_L) * 1.1mV / 1000 (active when VBUS present)
#define AXP2101_VBUS_VOLT_H         0x38
#define AXP2101_VBUS_VOLT_L         0x39
// E-Gauge fuel gauge percentage (datasheet REG A4H), 0-100
#define AXP2101_FUEL_GAUGE          0xA4

#define AXP2101_BATT_VOLT_MASK      0xFF
#define AXP2101_BATT_VOLT_SHIFT     8
#define AXP2101_VBUS_VOLT_MASK      0xFF
#define AXP2101_VBUS_VOLT_SHIFT     8

// AXP2101 Charge control registers (datasheet REG 62H, 67H)
// REG 62H: ICC charge current, bits[4:0]
//   N<=8: N*25 mA, N>8: 200+100*(N-8) mA.  0x0A=400mA, 0x0E=800mA, 0x10=1000mA
#define AXP2101_ICC_CHG_SET         0x62
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

// AXP2101 Status registers (datasheet REG 00H, 01H)
// REG 01H bits[2:0]: charge state (0=tri,1=pre,2=CC,3=CV,4=done,5=not charging)
#define AXP2101_STATUS1             0x00
#define AXP2101_STATUS2             0x01

// AXP2101 IRQ status registers (datasheet REG 48H-4AH), write-1-to-clear
// IRQ0 (0x48): VBUS insert/remove, battery over-voltage/under-voltage
// IRQ1 (0x49): battery insert/remove, POWERON key events
// IRQ2 (0x4A): bit 4=charge done, bit 3=charge start, bit 1=safety timer expire
#define AXP2101_IRQ_STATUS0         0x48
#define AXP2101_IRQ_STATUS1         0x49
#define AXP2101_IRQ_STATUS2         0x4A

// AXP192 Charge control registers
// REG 33H: Charge control 1
//   [7]   charge enable (1=on)
//   [6:5] target voltage: 00=4.1V, 01=4.15V, 10=4.2V, 11=4.36V
//   [4]   end current: 0=10%, 1=15%
//   [3:0] charge current: 0x3=360mA, 0x7=700mA, 0xB=1000mA
//         (0000=100,0001=190,0010=280,0011=360,...,1111=1320 mA)
#define AXP192_CHARGE1              0x33
// REG 34H: Charge control 2
//   [7:6] precharge timeout: 00=30m, 01=40m, 10=50m, 11=60m
//   [5:3] external channel current (300-1000mA, 100mA/step)
//   [2]   external channel enable during charging
//   [1:0] CC timeout: 00=7h, 01=8h, 10=9h, 11=10h
//   Note: timeouts are always active (no enable bit unlike AXP2101)
#define AXP192_CHARGE2              0x34
// REG 01H: bit 6=charging, bit 5=battery present
#define AXP192_MODE_CHGSTATUS       0x01
// AXP192 IRQ status registers (write-1-to-clear)
#define AXP192_IRQ_STATUS1          0x44
#define AXP192_IRQ_STATUS2          0x45
#define AXP192_IRQ_STATUS3          0x46
#define AXP192_IRQ_STATUS4          0x47

extern Status status;

// Singleton class managing the AXP PMU (Power Management Unit).
// Handles chip detection, rail configuration, battery monitoring,
// charge current/timeout configuration (battery pack dependent),
// and periodic status polling with IRQ clearing.
class Power {
public:
    static Power& getInstance()
    {
        static Power instance;
        return instance;
    }
    Power();

    // Detect and configure the AXP PMU chip. Called once from setup().
    // Probes I2C for AXP192/AXP2101, configures LDO voltages, charge
    // current and safety timeouts based on battery pack config (1P/2P/3P),
    // and enables ADC channels.
    void checkAXP();

    // Read battery voltage from PMU ADC.
    // AXP2101: 14-bit ADC via registers 0x34-0x35
    // AXP192: 12-bit ADC via registers 0x78-0x79
    // Returns voltage in volts (e.g. 3.85), or 0.0 if no PMU detected.
    float getBatteryVoltage();

    // Estimate battery state of charge (0-100%).
    // AXP2101: reads hardware fuel gauge (E-Gauge, register 0xA4)
    // AXP192: voltage-based linear estimate (3.2V=0%, 4.2V=100%)
    int getBatteryPercentage();

    // Read VBUS (USB input) voltage. AXP2101 only.
    float getVbusVoltage();

    // Control GNSS power rail (ALDO4 on Supreme, ALDO3 on V1.2).
    void setGnssPower(bool on);

    // Put sensors into deep sleep for power saving.
    void deepSleepSensors();

    // Periodic PMU status check, self-gated to run every 60 seconds.
    // Reads charge state, clears IRQ registers, logs status and
    // warns on safety timer expiry. Call from main loop.
    void checkPmuStatus();

    // Return human-readable charge state string for display/web UI.
    // e.g. "Charging (CC)", "Done", "Not charging", "Pre-charge"
    const char* getChargeStateStr();

    TwoWire* getPmuWire() { return pmuWire; }

    // Return detected AXP chip type: 0=none, 1=AXP192, 2=AXP2101
    uint8_t getAXPchip();

private:
    void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data);
    uint8_t I2CreadByte(uint8_t Address, uint8_t Register);
    void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data);
    TwoWire* pmuWire;
    unsigned long lastPmuCheck = 0;
};
#endif