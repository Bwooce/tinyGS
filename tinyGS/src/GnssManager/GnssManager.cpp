/*
  GnssManager.cpp - GNSS time sync and auto-location manager

  Copyright (C) 2020 -2021 @G4lile0, @gmag12 and @dev_4m1g0

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

#include "GnssManager.h"

#ifdef HAS_GNSS

#include "../Logger/Logger.h"
#include <time.h>
#include <sys/time.h>

// Sync every 24h minus 61 seconds (prime) to avoid locking to daily cycles
#define GNSS_SYNC_INTERVAL ((24 * 3600 - 61) * 1000)
#define GNSS_TIMEOUT (15 * 60 * 1000)         // 15 minutes max on time without fix

GnssManager::GnssManager() : serial(nullptr), enabled(false), powered(false), locationUpdated(false), fixLoggedThisCycle(false), lastSync(0), lastLocationUpdate(0), powerOnTime(0) {}

void GnssManager::begin() {
    board_t board;
    if (!ConfigManager::getInstance().getBoardConfig(board)) {
        Log::console(PSTR("GNSS: Failed to get board config"));
        return;
    }

    if (board.GNSS_RX != UNUSED && board.GNSS_TX != UNUSED) {
        Log::console(PSTR("GNSS: Initializing on RX:%d TX:%d"), board.GNSS_RX, board.GNSS_TX);

        // Wake up GNSS if pin is defined
        if (board.GNSS_WAKEUP != UNUSED) {
            pinMode(board.GNSS_WAKEUP, OUTPUT);
            digitalWrite(board.GNSS_WAKEUP, HIGH);
            Log::console(PSTR("GNSS: Wakeup pin %d set HIGH"), board.GNSS_WAKEUP);
        }

        Serial1.begin(9600, SERIAL_8N1, board.GNSS_RX, board.GNSS_TX);
        serial = &Serial1;
        enabled = true;

        // Initial Power On
        Power::getInstance().setGnssPower(true);
        powered = true;
        powerOnTime = millis();
    } else {
        Log::debug(PSTR("GNSS: No GNSS pins defined for this board"));
    }
}

void GnssManager::loop() {
    if (!enabled) return;

    checkPowerCycle();

    if (powered && serial) {
        while (serial->available() > 0) {
            gps.encode(serial->read());
        }

        static unsigned long lastStatusLog = 0;

        if (millis() - lastStatusLog > 30000) {
            Log::debug(PSTR("GNSS Status: %s, Sats: %d, HDOP: %d"),
                hasFix() ? "FIX" : "NO FIX",
                gps.satellites.value(),
                gps.hdop.value());
            lastStatusLog = millis();
        }

        if (hasFix()) {
            if (!fixLoggedThisCycle) {
                Log::console(PSTR("GNSS: Fix obtained. Sats: %d, HDOP: %d"), gps.satellites.value(), gps.hdop.value());
                fixLoggedThisCycle = true;
            }
            syncTime();

            // Auto Location Logic
            if (ConfigManager::getInstance().getAutoLocation()) {
                unsigned long interval = ConfigManager::getInstance().getGnssInterval() * 1000;
                bool shouldUpdate = (interval > 0 && millis() - lastLocationUpdate > interval) ||
                                    (interval == 0 && lastLocationUpdate == 0);

                if (shouldUpdate) {
                    ConfigManager::getInstance().setLatRuntime(getLatitude());
                    ConfigManager::getInstance().setLonRuntime(getLongitude());
                    lastLocationUpdate = millis();
                    locationUpdated = true;
                    Log::console(PSTR("GNSS: Auto-updated location: %.3f, %.3f"), getLatitude(), getLongitude());
                }
            } else {
                // Legacy behavior: Only update if 0.0/0.0
                static bool fixLogged = false;
                if (!fixLogged && ConfigManager::getInstance().getLatitude() == 0.0 && ConfigManager::getInstance().getLongitude() == 0.0) {
                    char latBuf[16], lonBuf[16];
                    snprintf(latBuf, sizeof(latBuf), "%.3f", getLatitude());
                    snprintf(lonBuf, sizeof(lonBuf), "%.3f", getLongitude());
                    ConfigManager::getInstance().setLat(latBuf);
                    ConfigManager::getInstance().setLon(lonBuf);
                    Log::console(PSTR("GNSS: Updated station location to current fix (Legacy)."));
                    fixLogged = true;
                }
            }
        }
    }
}

void GnssManager::checkPowerCycle() {
    unsigned long now = millis();
    bool autoLoc = ConfigManager::getInstance().getAutoLocation();
    unsigned long interval = ConfigManager::getInstance().getGnssInterval() * 1000;

    // Auto Location Power Management
    if (autoLoc) {
        if (interval > 0) {
            // Periodic Mode
            if (powered) {
                if (lastLocationUpdate > powerOnTime) {
                    Log::console(PSTR("GNSS: Location updated, powering down."));
                    Power::getInstance().setGnssPower(false);
                    powered = false;
                } else if (now - powerOnTime > GNSS_TIMEOUT) {
                    Log::console(PSTR("GNSS: Timeout (no fix), powering down."));
                    Power::getInstance().setGnssPower(false);
                    powered = false;
                }
            } else {
                if (now - lastLocationUpdate > interval) {
                    Log::console(PSTR("GNSS: Waking for periodic update."));
                    Power::getInstance().setGnssPower(true);
                    powered = true;
                    powerOnTime = now;
                    fixLoggedThisCycle = false;
                }
            }
            return;
        } else {
            // Interval 0 with AutoLoc ON means "Always On"
            if (!powered) {
                 Power::getInstance().setGnssPower(true);
                 powered = true;
                 powerOnTime = now;
            }
            return;
        }
    }

    // Legacy Power Management (Time Sync Only)
    if (powered) {
        if (now - lastSync < 60000 && lastSync != 0) {
             Log::console(PSTR("GNSS: Time synced, powering down."));
             Power::getInstance().setGnssPower(false);
             powered = false;
        }
        else if (now - powerOnTime > GNSS_TIMEOUT) {
            Log::console(PSTR("GNSS: Timeout (no fix), powering down."));
            Power::getInstance().setGnssPower(false);
            powered = false;
        }
    }
    else {
        if (now - lastSync > GNSS_SYNC_INTERVAL) {
            Log::console(PSTR("GNSS: Waking up for daily sync."));
            Power::getInstance().setGnssPower(true);
            powered = true;
            powerOnTime = now;
            fixLoggedThisCycle = false;
        }
    }
}

bool GnssManager::hasFix() {
    return gps.location.isValid() && gps.location.age() < 2000;
}

uint32_t GnssManager::getSatellites() {
    return gps.satellites.value();
}

double GnssManager::getLatitude() {
    return gps.location.lat();
}

double GnssManager::getLongitude() {
    return gps.location.lng();
}

double GnssManager::getAltitude() {
    return gps.altitude.meters();
}

void GnssManager::syncTime() {
    if (!gps.time.isValid() || !gps.date.isValid()) return;

    // Debounce: only sync once per minute
    if (millis() - lastSync < 60000 && lastSync != 0) return;

    // Howard Hinnant's days_from_civil algorithm (public domain)
    // Converts a Gregorian date to days since 1970-01-01 (Unix epoch)
    int y = gps.date.year();
    unsigned m = gps.date.month();
    unsigned d = gps.date.day();
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097L + static_cast<long>(doe) - 719468L;

    time_t timeVal = days * 86400L +
            gps.time.hour() * 3600L + gps.time.minute() * 60L + gps.time.second();

    struct timeval now = { .tv_sec = timeVal, .tv_usec = 0 };
    settimeofday(&now, NULL);

    Log::console(PSTR("GNSS: Time synced to %04d-%02d-%02d %02d:%02d:%02d UTC"),
                 gps.date.year(), gps.date.month(), gps.date.day(),
                 gps.time.hour(), gps.time.minute(), gps.time.second());
    lastSync = millis();
}

bool GnssManager::isSleeping() {
    return enabled && !powered;
}

#endif // HAS_GNSS
