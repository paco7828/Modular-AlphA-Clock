# GeniClock

A retro alphanumeric clock built around the ESP32-C3 SuperMini, with GPS-synchronized RTC timekeeping, a timer, an alarm, and temperature display. Designed to accept two different display families (HDSP-2111 / HDLY-2416) via a runtime-switchable configuration.

## Hardware

| Component | Role |
|---|---|
| **ESP32-C3 SuperMini** | Main microcontroller |
| **MCP23017** (I2C, `0x20`) | GPIO expander driving the display(s) — parallel data/address/control lines |
| **DS3231M** RTC + CR2032 holder | Real-time clock, battery-backed to survive power loss |
| **GPS module header** | UART, for time sync and speed-o-meter |
| **Analog joystick** (X/Y/SW) | Navigation, value setting, button functions |
| **Buzzer** | Audio feedback: startup melody, button clicks, hourly chime, timer/alarm |
| **Dual DIP switch** | 1: connects an external power supply to the board · 2: mutes/enables the buzzer |
| **2× THT pads** | External power supply wiring points |
| **Display sockets (2 variants)** | **A)** 1× HDSP-2111 (8-digit) · **B)** 2× HDLx-2416 or DLx-2416 (4+4 = 8 digits) |

Display type selection: `DEFAULT_CLOCK_TYPE` in `constants.h`, overridable via NVS (`Preferences`, key `clockType`), with a boot-time procedure (`runDisplayTypeSetup()`) for manual switching — 5 short joystick-button presses toggles the type, a 2-second hold confirms.

## Firmware structure

```
geni-code.ino       – main state machine, setup()/loop(), joystick routing, mode switching
constants.h          – pin mapping, timing constants, melodies, display strings
HDSPDisplay.h         – MCP23017-based driver for both display types (clockType flag)
gps.h                 – TinyGPSPlus wrapper, UTC→Hungarian local time conversion (CET/CEST, Zeller's congruence)
Better-JoyStick.h     – analog joystick → 8-direction + button, with deadzone handling
timer.h               – countdown timer state machine
alarm.h               – daily repeating alarm state machine (NVS-persisted)
settime.h             – manual RTC time set (triggered by a triple UP-press)
```

### Modes (`currentMode`, joystick RIGHT/LEFT cycles through them)

| # | Mode | Description |
|---|---|---|
| 0 | `HH:MM:SS` | Time (JS button: reverse order to `SS:MM:HH`) |
| 1 | `YYYY. MM` | Year/month |
| 2 | `DD Www` | Day + short weekday name |
| 3 | Temperature | From the DS3231's internal sensor (°C/°F toggle via JS button) |
| 4 | Timer | Set hours/minutes/seconds → countdown → melody |
| 5 | Alarm | Set hours/minutes + on/off, persisted to NVS |
| 6 | GPS speed | km/h, GPS boosted to 5 Hz while this mode is active |

### Time source logic

The **DS3231 RTC is the single source of truth** for what the display actually shows (`updateTimeSource()`, read every 1s). GPS only periodically (`GPS_RTC_SYNC_INTERVAL` = 60s) resyncs the RTC when it has a fix — this protects against RTC drift without making the displayed time dependent on GPS's momentary availability. If no GPS is wired in, `hasFix()` simply stays false and this path is inert.

Manual time setting (`settime.h`) is reached via a triple UP-press from any normal mode; seconds are zeroed at the moment of the final CONFIRM, allowing a precise sync to a real-world top-of-minute.

### Button functions in timer/alarm mode

Physical direction → logical function (the joystick's VRX/VRY axes are physically swapped relative to `getDirection()`'s naming — noted in code comments):

- **RIGHT** → CONFIRM (accept value / advance to next field)
- **LEFT** → CANCEL (single press: reset current field to 0, double press in quick succession: exit the mode)
- **UP** → ADD
- **DOWN** → SUBTRACT