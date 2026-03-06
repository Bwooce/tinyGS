# autoLowPower Design

## Overview

Efficient power management for stationary, solar-powered TinyGS ground stations.
When enabled, the station operates in a reduced-power idle mode with deeper sleep
during known satellite gaps, while keeping MQTT alive for server retune commands.

## Config

All options in `advancedConfig` JSON, appended to existing fields (no NVS version bump).

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `lowPower` | bool | false | Manual force low-power (existing, unchanged) |
| `autoLowPower` | bool | false | Stationary station efficient idle mode |
| `wakeTimeout` | int | 300 | Seconds to stay fully active after PWR button press |

Example: `{"autoLowPower": true, "wakeTimeout": 300}`

## Power States

| State | WiFi | MQTT | OLED | GNSS | Radio | CPU | ~Current |
|-------|------|------|------|------|-------|-----|----------|
| Full active | on | on | on | on | RX | 240MHz | ~120mA |
| Idle low power | modem sleep | alive | off | off | RX | 80MHz | ~15-20mA |
| Pass sleep | off | off | off | off | RX (ext0) | light sleep | ~1-2mA |

## Triggers

- `autoLowPower: true` - enables the two-tier low-power behavior below
- `lowPower: true` - existing manual flag, keeps current MQTT-failure deep sleep behavior

## State Transitions

### Full active -> Idle low power
When `autoLowPower` is enabled and no override is active:
- Enable WiFi modem sleep (`esp_wifi_set_ps(WIFI_PS_MIN_MODEM)`)
- Turn off GNSS (`setGnssPower(false)`)
- Turn off OLED (display timeout applies)
- Reduce CPU to 80MHz (`setCpuFrequencyMhz(80)`)
- MQTT stays connected, server retune commands received immediately

### Idle low power -> Pass sleep
When ALL of:
- Have valid TLE (`status.modeminfo.tle[0] != 0`)
- Satellite below horizon (`status.tle.dSatEL <= 0`)
- Next rise > wakeTimeout seconds away (from PassPredictor)

Actions:
- Configure ext0 wake on radio IRQ pin (wake on packet)
- Configure timer wake for min(time_to_rise - 120s, 30 min)
- Disconnect WiFi, enter esp_light_sleep_start()

### Pass sleep -> Idle low power
On wake (timer or ext0):
- Reconnect WiFi/MQTT
- Stay in idle low power for minimum 60s before considering pass sleep again
- Log wake reason

### Any -> Full active (temporary)
PWR button short press (via AXP2101 IRQ):
- Reset WAKE_TIMEOUT timer
- Turn OLED on, GNSS on, CPU 240MHz, disable modem sleep
- After WAKE_TIMEOUT expires with no further button press, return to idle low power

### Idle low power -> Full active (permanent)
- `autoLowPower` disabled via config change
- Device reboot

## Key Behaviors

1. **MQTT stays alive in idle low power** - modem sleep preserves TCP connections.
   Server retune commands arrive with ~100-200ms added latency.

2. **No-TLE satellites** - log assignment, stay in idle low power. Cannot predict
   passes so never enter pass sleep. Wait for server to assign something with TLE.

3. **Pass sleep only with TLE confidence** - need valid TLE and known gap duration
   before disconnecting WiFi.

4. **WAKE_TIMEOUT shared with display** - replaces hardcoded DISPLAY_TIMEOUT (300s).
   PWR button press resets both the display and the stay-awake timer.

5. **GNSS off** - station is stationary, position known. Mobile stations should not
   enable autoLowPower.

## TODOs

- [ ] Verify ext0 (radio IRQ) wake from light sleep actually works
- [ ] Verify RTC timer wake from light sleep works correctly
- [ ] Test modem sleep MQTT keepalive reliability over extended periods

## Implementation Order

1. Parse new advancedConfig fields (autoLowPower, wakeTimeout)
2. Parameterize DISPLAY_TIMEOUT -> WAKE_TIMEOUT
3. Add PWR button short-press detection in checkPmuStatus() IRQ handler
4. Implement idle low power state (modem sleep, peripherals off, CPU scaling)
5. Refactor interPassSleep() into pass sleep tier (only with valid TLE)
6. Integration test: autoLowPower + satellite transitions
