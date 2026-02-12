/*
  GnssManager.h - GNSS time sync and auto-location manager

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
#ifndef GnssManager_h
#define GnssManager_h

#ifdef HAS_GNSS

#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "../ConfigManager/ConfigManager.h"
#include "../Status.h"
#include "../Power/Power.h"

class GnssManager {
public:
    static GnssManager& getInstance() {
        static GnssManager instance;
        return instance;
    }

    void begin();
    void loop();
    bool hasFix();
    uint32_t getSatellites();
    double getLatitude();
    double getLongitude();
    double getAltitude();
    void syncTime();
    bool isEnabled() { return enabled; }
    bool isSleeping();
    bool isLocationUpdated() { return locationUpdated; }
    void clearLocationUpdated() { locationUpdated = false; }

private:
    GnssManager();
    void checkPowerCycle();
    TinyGPSPlus gps;
    HardwareSerial* serial;
    bool enabled;
    bool powered;
    bool locationUpdated;
    bool fixLoggedThisCycle;
    unsigned long lastSync;
    unsigned long lastLocationUpdate;
    unsigned long powerOnTime;
};

#else

// Stub for builds without GNSS hardware (TinyGPSPlus not available)
class GnssManager {
public:
    static GnssManager& getInstance() { static GnssManager instance; return instance; }
    void begin() {}
    void loop() {}
    bool hasFix() { return false; }
    uint32_t getSatellites() { return 0; }
    double getLatitude() { return 0.0; }
    double getLongitude() { return 0.0; }
    double getAltitude() { return 0.0; }
    void syncTime() {}
    bool isEnabled() { return false; }
    bool isSleeping() { return false; }
    bool isLocationUpdated() { return false; }
    void clearLocationUpdated() {}
};

#endif
#endif
