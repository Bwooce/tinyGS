#pragma once
#include "../Status.h"
#include <AioP13.h>
#include "../ConfigManager/ConfigManager.h"
#include <time.h>

extern Status status;

// Returns seconds until the current satellite next rises above the horizon.
// Scans up to max_hours ahead in 1-minute steps, refines to 10-second steps.
// Returns 0 if satellite is already above horizon.
// Returns max_hours*3600 if no TLE available or no rise found within the scan window.
inline uint32_t secondsUntilNextRise(uint8_t max_hours = 24) {
  // No TLE data -- can't predict, caller decides what to do
  if (status.modeminfo.tle[0] == 0)
    return max_hours * 3600;

  // Already above horizon
  if (status.tle.dSatEL > 0)
    return 0;

  float lat = ConfigManager::getInstance().getLatitude();
  float lon = ConfigManager::getInstance().getLongitude();
  double alt = status.tle.tgsALT;

  time_t now = time(NULL);
  struct tm *t = gmtime(&now);

  P13Observer qth("tinyGS", lat, lon, alt);
  P13Satellite_tGS sat(status.modeminfo.tle);
  P13DateTime scanTime(1900 + t->tm_year, 1 + t->tm_mon, t->tm_mday,
                        t->tm_hour, t->tm_min, t->tm_sec);

  // Coarse scan: 1-minute steps
  uint32_t max_minutes = (uint32_t)max_hours * 60;
  double el, az;
  uint32_t rise_minute = 0;

  for (uint32_t m = 1; m <= max_minutes; m++) {
    scanTime.add(1.0 / 1440.0);  // +1 minute
    sat.predict(scanTime);
    sat.elaz(qth, el, az);
    if (el >= 0) {
      rise_minute = m;
      break;
    }
  }

  if (rise_minute == 0)
    return max_hours * 3600;  // no rise found

  // Fine scan: back up 1 minute, scan in 10-second steps
  t = gmtime(&now);
  P13DateTime fineTime(1900 + t->tm_year, 1 + t->tm_mon, t->tm_mday,
                        t->tm_hour, t->tm_min, t->tm_sec);
  uint32_t base_seconds = (rise_minute - 1) * 60;
  fineTime.add((double)base_seconds / 86400.0);

  for (uint32_t s = 0; s <= 60; s += 10) {
    sat.predict(fineTime);
    sat.elaz(qth, el, az);
    if (el >= 0)
      return base_seconds + s;
    fineTime.add(10.0 / 86400.0);  // +10 seconds
  }

  // Should not reach here, but return coarse estimate
  return rise_minute * 60;
}
