# LilyGo T-Beam Supreme (S3) Setup for TinyGS

The LilyGo T-Beam Supreme is an ESP32-S3 based board with an SX1262 LoRa radio, AXP2101 PMU, and an integrated GPS (Ublox M10 or L76K).

## Pin Mapping (Verified)

| Function | Pin (GPIO) |
| :--- | :--- |
| **LoRa CS (NSS)** | 10 |
| **LoRa DIO1** | 1 |
| **LoRa BUSY** | 4 |
| **LoRa RESET** | 5 |
| **LoRa SCK** | 12 |
| **LoRa MISO** | 13 |
| **LoRa MOSI** | 11 |
| **I2C SDA** | 17 |
| **I2C SCL** | 18 |
| **GPS TX** | 8 |
| **GPS RX** | 9 |
| **GPS Wakeup** | 7 |
| **PMU SDA** | 42 (Wire1) |
| **PMU SCL** | 41 (Wire1) |
| **User Button** | 0 |
| **Green LED** | 3 |

## Features

### 1. Power Management (AXP2101)
- Full support for AXP2101 PMU via Wire1.
- Correct battery voltage reading (1mV/bit).
- Accurate fuel gauge percentage.
- Automatic power control for Radio and GPS.

### 2. Display (SH1106)
- SH1106 OLED driver (vs SSD1306 on other boards).
- Auto-off after 5 minutes of inactivity to save power.
- Wakes on button press or serial input.

### 3. Power Saving
- **Shared Rail (ALDO1)**: Powers the OLED and onboard sensors (QMC5883L Compass, BME280). This rail remains active even when the display is off.
- **Radio Rail (ALDO3)**: Powers the SX1262 LoRa module.
- **GPS Rail (ALDO4)**: Powers the GNSS module independently.
- **Unused Rails**: Unused PMU rails (ALDO2, BLDO1/2, DLDO1/2) are explicitly disabled to minimize quiescent current.

### 4. GNSS Auto-Location
- Automatic time sync from GPS (no NTP dependency for initial time).
- Optional auto-location: enable "Auto Location (GNSS)" in config to update station coordinates from GPS fix.
- Configurable update interval (default 3600s). Set to 0 for continuous updates.
- Power-managed: GNSS module powers down between updates to save battery.
- Legacy mode (auto-location off): only sets coordinates once if station is at 0.0/0.0.

## Configuration

1. Connect to the TinyGS Access Point.
2. Navigate to `http://192.168.4.1/config`.
3. Select **433 Mhz LilyGo T-Beam Supreme** as the board type.
4. (Optional) Enable **Auto Location (GNSS)** and set the update interval.
5. Save and Restart.

## Troubleshooting

- **No Packets**: Ensure you are using a 433MHz or 868/915MHz antenna matching your board variant. The SX1262 pins are identical for all frequency variants.
- **Battery 0.00V**: Ensure the board is selected as "Supreme" to enable the AXP2101 driver.
