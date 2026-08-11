#include "constants.h"
#include "HDSPDisplay.h"
#include "gps.h"
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>
#include "timer.h"
#include "alarm.h"
#include "settime.h"
#include "Better-JoyStick.h"

// Joystick
BetterJoystick joystick;

// Joystick állapot (debounce + edge detect)
byte lastJsDirection = 0;
unsigned long lastJsDebounceTime = 0;
bool jsDirFired = false;
unsigned long lastJsBtnDebounceTime = 0;
bool jsButtonState = false;
bool lastJsButtonState = false;

// Triple press (confirm = JS gomb)
unsigned long firstConfirmPress = 0;
byte confirmPressCount = 0;

struct timeData {
  int year;
  byte month;
  byte day;
  byte dayIndex;
  byte hour;
  byte minute;
  byte second;
};

// Time & Date values storage
timeData currentTime;

byte currentMode = 0;  // Start with HH:MM:SS mode
/*
0 -> time
1 -> year+month
2 -> day+short day name
3 -> temperature
4 -> timer
5 -> alarm
6 -> GPS speed
*/

// Per-mode display format toggles (JS button click while browsing, modes 0-3)
bool timeDisplayReversed = false;  // mode 0: HH:MM:SS <-> SS:MM:HH
bool dateDisplayReversed = false;  // mode 1: YYYY. MM <-> MM. YYYY
bool dayDisplayReversed = false;   // mode 2: DD Www   <-> Www DD
bool tempFahrenheit = false;       // mode 3: Celsius  <-> Fahrenheit

// GPS init deferred out of setup() - see gpsInitPending in setup()/loop()
bool gpsInitPending = false;

// Mode title display state
bool showingModeTitle = false;
unsigned long modeTitleStartTime = 0;

// Time source status flags
bool gpsAvailable = false;
bool rtcAvailable = false;

// Status message variables
bool showingStatusMessage = false;
unsigned long statusMessageStart = 0;

// Timing for RTC / GPS sync
unsigned long lastGpsRtcSync = 0;
unsigned long lastRtcRead = 0;
unsigned long lastDisplayUpdate = 0;

// Temperature reading variables
unsigned long lastTemperatureRead = 0;
float currentTemperature = 0.0;

// Display update interval
unsigned long displayUpdateInterval = 500;  // milliseconds

// Hour notification variables
byte lastHour = 255;  // Initialize to invalid value to avoid notification on startup
bool hourNotificationEnabled = true;
bool playingHourNotification = false;
unsigned long notificationStartTime = 0;
byte notificationStep = 0;

// Startup sound variables
bool playingStartupSound = false;
unsigned long startupSoundStartTime = 0;
byte startupSoundStep = 0;

// Manual time set mode (triggered by triple UP-press)
bool inSetTimeMode = false;

// Display
HDSPDisplay HDSP;  // default 0x20

// GPS
GPS gps;

// RTC
RTC_DS3231 rtc;

// Preferences for persistent storage
Preferences preferences;

// Timer
Timer timer(&HDSP, BUZZER);

// Alarm - RENAMED from 'alarm' to 'alarmClock' to avoid conflict with system alarm() function
Alarm alarmClock(&HDSP, BUZZER, &preferences);

// Manual time set (writes into the DS3231 - no GPS wiring required)
SetTime setTime(&HDSP, BUZZER);

void setup() {
  // Initialize preferences
  preferences.begin("modular-alpha", false);

  // I2C init
  Wire.begin(I2C_SDA, I2C_SCL);

  // Display
  runDisplayTypeSetup();
  HDSP.displayText(" Alph-A ");

  // Initialize time structure to prevent 00:00:00 display
  currentTime.year = 2025;
  currentTime.month = 1;
  currentTime.day = 1;
  currentTime.dayIndex = 3;  // Wednesday
  currentTime.hour = 0;
  currentTime.minute = 0;
  currentTime.second = 0;

  // Joystick
  joystick.begin(JS_SW, JS_X, JS_Y);

  // Buzzer
  pinMode(BUZZER, OUTPUT);

  // GPS
  gpsInitPending = true;

  if (rtc.begin()) {
    rtcAvailable = true;

    // Immediately read RTC time to avoid 00:00:00 display
    DateTime now = rtc.now();
    if (now.isValid()) {
      currentTime.year = now.year();
      currentTime.month = now.month();
      currentTime.day = now.day();
      currentTime.dayIndex = now.dayOfTheWeek();
      currentTime.hour = now.hour();
      currentTime.minute = now.minute();
      currentTime.second = now.second();

      // Initialize lastRtcRead so updateTimeSource doesn't wait
      lastRtcRead = millis();
    }
  }
  // RTC FAIL will be shown after startup sequence

  // Start startup sound and display sequence
  playingStartupSound = true;
  startupSoundStartTime = millis();
  startupSoundStep = 0;

  // Title is already shown, play first startup tone
  tone(BUZZER, STARTUP_MELODY[0], STARTUP_NOTE_DURATIONS[0]);

  // Initialize display update timer
  lastDisplayUpdate = millis();
}

void loop() {
  if (!playingStartupSound && alarmClock.isAlarmActive()) {
    alarmClock.update();
  }

  // Handle manual time-set mode (entered via triple UP-press, see handleJoystick())
  if (inSetTimeMode) {
    handleSetTimeJoystick();
    setTime.update();

    if (setTime.shouldExitSetTimeMode()) {
      bool commit = setTime.shouldCommitTime();
      int newYear = setTime.getYear();
      byte newMonth = setTime.getMonth();
      byte newDay = setTime.getDay();
      byte newHour = setTime.getHour();
      byte newMinute = setTime.getMinute();

      setTime.clearExitFlag();
      inSetTimeMode = false;
      startModeSwitch(0);

      if (commit && rtcAvailable) {
        rtc.adjust(DateTime(newYear, newMonth, newDay, newHour, newMinute, 0));
        lastRtcRead = 0;  // force an immediate re-read so the display reflects it right away
        showStatusMessage("TIME SET");
      }
    }
    return;
  }

  // Reset confirm press counter if timeout exceeded
  if (confirmPressCount > 0 && (millis() - firstConfirmPress > TRIPLE_PRESS_WINDOW)) {
    confirmPressCount = 0;
    firstConfirmPress = 0;
  }

  // Handle startup sound first
  if (playingStartupSound) {
    handleStartupSound();
    return;  // Don't do anything else during startup sound
  }

  // GPS init deferred here so a bad GPS_TX/GPS_RX config can't block the boot melody
  if (gpsInitPending) {
    gpsInitPending = false;
    gps.begin(GPS_RX, GPS_TX);
  }

  handleJoystick();

  // Check if timer wants to exit to main mode
  if (currentMode == 4 && timer.shouldExitTimerMode()) {
    timer.clearExitFlag();
    currentMode = 0;  // Go back to HH:MM:SS mode
    startModeSwitch(0);
    return;
  }

  // Check if alarm wants to exit to main mode
  if (currentMode == 5 && alarmClock.shouldExitAlarmMode()) {
    alarmClock.clearExitFlag();
    currentMode = 0;  // Go back to HH:MM:SS mode
    startModeSwitch(0);
    return;
  }

  // Update time source (GPS resyncs the RTC periodically; RTC drives the display)
  updateTimeSource();

  // Update temperature reading (only when in temperature mode)
  if (currentMode == 3) {
    updateTemperature();
  }

  // Check for alarm trigger (always check when alarm is enabled)
  if (alarmClock.isAlarmEnabled()) {
    alarmClock.checkAlarmTrigger(currentTime.hour, currentTime.minute, currentTime.second);
  }

  // Handle hour notification
  handleHourNotification();

  // Check if mode title should auto-confirm
  if (showingModeTitle && (millis() - modeTitleStartTime >= TITLE_SHOW_TIME)) {
    showingModeTitle = false;

    // For timer mode, trigger the setting title display
    if (currentMode == 4) {
      timer.forceShowSettingTitle();
    }

    // For alarm mode, trigger the setting title display
    if (currentMode == 5) {
      alarmClock.forceShowSettingTitle();
    }

    // Set appropriate update interval for selected mode
    switch (currentMode) {
      case 1: displayUpdateInterval = 1000; break;  // year+month updates every 1s
      case 2: displayUpdateInterval = 1000; break;  // day+name updates every 1s
      case 3: displayUpdateInterval = 2000; break;  // temperature updates every 2s
      case 4:
        // Timer mode - handle separately, no need to set interval
        break;
      case 5:
        // Alarm mode - handle separately, no need to set interval
        break;
      case 6: displayUpdateInterval = 200; break;  // GPS speed updates every 200ms
    }

    // Force immediate display update by setting lastDisplayUpdate to 0
    lastDisplayUpdate = 0;
  }

  // Update display based on current state
  if (!showingModeTitle && !playingHourNotification) {
    // In normal mode - update display based on current mode
    updateTDDisplay();
  }
}

void updateTemperature() {
  // Read temperature from RTC
  if (rtcAvailable && (millis() - lastTemperatureRead >= TEMPERATURE_READ_INTERVAL)) {
    lastTemperatureRead = millis();
    currentTemperature = rtc.getTemperature();
  }
}

void handleStartupSound() {
  // Check if it's time to play the next note
  if (millis() - startupSoundStartTime >= STARTUP_NOTE_DURATIONS[startupSoundStep]) {
    startupSoundStep++;

    if (startupSoundStep < STARTUP_MELODY_LENGTH) {
      // Play next note in startup melody
      startupSoundStartTime = millis();
      tone(BUZZER, STARTUP_MELODY[startupSoundStep], STARTUP_NOTE_DURATIONS[startupSoundStep]);
    } else {
      // Startup sound finished
      playingStartupSound = false;
      noTone(BUZZER);

      // Show status messages after startup sound
      if (rtcAvailable) {
        showStatusMessage(" RTC OK ");
      } else {
        showStatusMessage("RTC FAIL");  // Show RTC FAIL after startup
      }

      // Wait a bit before starting normal operation
      delay(500);

      // Force immediate display update
      lastDisplayUpdate = 0;
    }
  }
}

void handleHourNotification() {
  // Check if we should start a new hour notification
  if (hourNotificationEnabled && !playingHourNotification && lastHour != 255 && currentTime.hour != lastHour && currentTime.minute == 0 && currentTime.second == 0) {

    // Start hour notification
    playingHourNotification = true;
    notificationStartTime = millis();
    notificationStep = 0;

    // Play first tone
    tone(BUZZER, NOTIF_MELODY[0], NOTIF_STEP_DURATION);

    // Show hour notification on display
    char hourMsg[9];
    sprintf(hourMsg, " %02d:00  ", currentTime.hour);
    HDSP.forceDisplayText(hourMsg);
  }

  // Handle notification progression
  if (playingHourNotification) {
    if (millis() - notificationStartTime >= (notificationStep + 1) * NOTIF_STEP_DURATION) {
      notificationStep++;

      if (notificationStep < NOTIF_MELODY_LENGTH) {
        // Play next tone
        tone(BUZZER, NOTIF_MELODY[notificationStep], NOTIF_STEP_DURATION);
      } else {
        // Notification finished
        playingHourNotification = false;
        noTone(BUZZER);

        // Force display update by resetting timer
        lastDisplayUpdate = 0;
      }
    }
  }

  // Update lastHour for next comparison
  lastHour = currentTime.hour;
}

// Simplified status message function
void showStatusMessage(const char *message) {
  showingStatusMessage = true;
  statusMessageStart = millis();
  HDSP.forceDisplayText((char *)message);
}

void startModeSwitch(byte newMode) {
  if (newMode > MAX_MODE) newMode = MIN_MODE;

  if (newMode == 6 && currentMode != 6) {
    gps.setUpdateRate(GPS_RATE_HIGH_MS);
  } else if (newMode != 6 && currentMode == 6) {
    gps.setUpdateRate(GPS_RATE_NORMAL_MS);
  }

  currentMode = newMode;
  showingModeTitle = true;
  modeTitleStartTime = millis();

  if (newMode == 4) {
    timer.reset();
  }

  if (newMode == 5) {
    alarmClock.reset();
  }

  HDSP.displayText(MODE_TITLES[currentMode]);
}

void playButtonBeep(byte buttonIndex) {
  tone(BUZZER, BUTTON_BEEP_FREQS[buttonIndex], BUTTON_BEEP_DURATION);
}

void handleJoystick() {
  unsigned long now = millis();

  if (alarmClock.isAlarmActive()) {
    byte dir = joystick.getDirection();
    bool btnRaw = joystick.getButtonPress();
    if ((dir != 0 || btnRaw) && (now - lastJsDebounceTime >= DEBOUNCE_DELAY)) {
      lastJsDebounceTime = now;
      alarmClock.handleConfirmButton();
    }
    lastJsDirection = dir;
    jsButtonState = btnRaw;
    lastJsButtonState = btnRaw;
    return;
  }

  // A joystick VRX/VRY bekötése fizikailag fel-le/balra-jobbra van cserélve a
  // getDirection() elnevezéséhez képest: fizikai JOBBRA -> dir==4 (kód "UP"),
  // fizikai FEL -> dir==2 (kód "RIGHT"), fizikai BALRA -> dir==3 (kód "DOWN"),
  // fizikai LE -> dir==1 (kód "LEFT"). A kommentek ezért a FIZIKAI irányt írják,
  // a dir==N csak a getDirection() belső számozása.
  byte dir = joystick.getDirection();
  if (dir != lastJsDirection) {
    lastJsDebounceTime = now;
    lastJsDirection = dir;
    jsDirFired = false;
  }
  if (dir != 0) {
    unsigned long requiredWait = jsDirFired ? JOYSTICK_REPEAT_INTERVAL : DEBOUNCE_DELAY;
    if (now - lastJsDebounceTime >= requiredWait) {
      lastJsDebounceTime = now;
      jsDirFired = true;

      if (dir == 4) {  // fizikai JOBBRA → CONFIRM (timer/alarm) / mode forward (böngészés)
        if (currentMode == 4 && !showingModeTitle) {
          timer.handleConfirmButton();
          return;
        }
        if (currentMode == 5 && !showingModeTitle) {
          alarmClock.handleConfirmButton();
          return;
        }
        playButtonBeep(2);
        byte newMode = (currentMode + 1 > MAX_MODE) ? MIN_MODE : currentMode + 1;
        startModeSwitch(newMode);
      } else if (dir == 3) {  // fizikai BALRA → CANCEL (timer/alarm) / mode backward (böngészés)
        playButtonBeep(1);
        if (currentMode == 4 && !showingModeTitle) {
          timer.handleCancelButton();
          if (timer.shouldExitTimerMode()) {
            timer.clearExitFlag();
            currentMode = 0;
            startModeSwitch(0);
          }
          return;
        }
        if (currentMode == 5 && !showingModeTitle) {
          alarmClock.handleCancelButton();
          if (alarmClock.shouldExitAlarmMode()) {
            alarmClock.clearExitFlag();
            currentMode = 0;
            startModeSwitch(0);
          }
          return;
        }
        byte newMode = (currentMode == MIN_MODE) ? MAX_MODE : currentMode - 1;
        startModeSwitch(newMode);
      } else if (dir == 2) {  // fizikai FEL → ADD (timer/alarm) / hármas nyomás (SetTime belépő)
        if (currentMode == 4 && !showingModeTitle) {
          playButtonBeep(2);
          timer.handleAddButton();
          return;
        }
        if (currentMode == 5 && !showingModeTitle) {
          playButtonBeep(2);
          alarmClock.handleAddButton();
          return;
        }
        // Triple press → manual time set (FEL 3x) - szándékosan néma, hogy a
        // számolgatás közben ne csippanjon minden egyes próbálkozásnál
        if (now - firstConfirmPress <= TRIPLE_PRESS_WINDOW) {
          confirmPressCount++;
          if (confirmPressCount >= 3) {
            enterSetTimeMode();
            confirmPressCount = 0;
            firstConfirmPress = 0;
          }
        } else {
          confirmPressCount = 1;
          firstConfirmPress = now;
        }
      } else if (dir == 1) {  // fizikai LE → SUBTRACT (timer/alarm) / óránkénti csengetés ki/be
        if (currentMode == 4 && !showingModeTitle) {
          playButtonBeep(3);
          timer.handleSubtractButton();
          return;
        }
        if (currentMode == 5 && !showingModeTitle) {
          playButtonBeep(3);
          alarmClock.handleSubtractButton();
          return;
        }
        playButtonBeep(3);
        hourNotificationEnabled = !hourNotificationEnabled;
        showStatusMessage(hourNotificationEnabled ? "CHM ON" : "CHM OFF");
        if (!hourNotificationEnabled && playingHourNotification) {
          playingHourNotification = false;
          noTone(BUZZER);
        }
      }
    }
  }

  // --- Gomb (JS SW) debounce ---
  bool btnRaw = joystick.getButtonPress();
  if (btnRaw != lastJsButtonState) lastJsBtnDebounceTime = now;
  if ((now - lastJsBtnDebounceTime) > DEBOUNCE_DELAY) {
    if (btnRaw != jsButtonState) {
      jsButtonState = btnRaw;
      if (jsButtonState) {
        playButtonBeep(0);
        if (currentMode == 4 && !showingModeTitle) {
          timer.handleConfirmButton();
        } else if (currentMode == 5 && !showingModeTitle) {
          alarmClock.handleConfirmButton();
        } else if (currentMode == 0) {
          timeDisplayReversed = !timeDisplayReversed;
          lastDisplayUpdate = 0;
        } else if (currentMode == 1) {
          dateDisplayReversed = !dateDisplayReversed;
          lastDisplayUpdate = 0;
        } else if (currentMode == 2) {
          dayDisplayReversed = !dayDisplayReversed;
          lastDisplayUpdate = 0;
        } else if (currentMode == 3) {
          tempFahrenheit = !tempFahrenheit;
          lastDisplayUpdate = 0;
        }
      }
    }
  }
  lastJsButtonState = btnRaw;
}

void enterSetTimeMode() {
  inSetTimeMode = true;
  setTime.reset(currentTime.year, currentTime.month, currentTime.day, currentTime.hour, currentTime.minute);
}

// Same debounce pattern as handleJoystick(), routed to the SetTime instance instead of mode/timer/alarm
void handleSetTimeJoystick() {
  unsigned long now = millis();

  byte dir = joystick.getDirection();
  if (dir != lastJsDirection) {
    lastJsDebounceTime = now;
    lastJsDirection = dir;
    jsDirFired = false;
  }
  if (dir != 0) {
    unsigned long requiredWait = jsDirFired ? JOYSTICK_REPEAT_INTERVAL : DEBOUNCE_DELAY;
    if (now - lastJsDebounceTime >= requiredWait) {
      lastJsDebounceTime = now;
      jsDirFired = true;

      if (dir == 4) {  // fizikai JOBBRA → CONFIRM
        playButtonBeep(0);
        setTime.handleConfirmButton();
      } else if (dir == 3) {  // fizikai BALRA → CANCEL (egyszer = mező reset, duplán = megszakít)
        playButtonBeep(1);
        setTime.handleCancelButton();
      } else if (dir == 2) {  // fizikai FEL → ADD
        playButtonBeep(2);
        setTime.handleAddButton();
      } else if (dir == 1) {  // fizikai LE → SUBTRACT
        playButtonBeep(3);
        setTime.handleSubtractButton();
      }
    }
  }

  // JS button (SW) - also CONFIRM
  bool btnRaw = joystick.getButtonPress();
  if (btnRaw != lastJsButtonState) lastJsBtnDebounceTime = now;
  if ((now - lastJsBtnDebounceTime) > DEBOUNCE_DELAY) {
    if (btnRaw != jsButtonState) {
      jsButtonState = btnRaw;
      if (jsButtonState) {
        playButtonBeep(0);
        setTime.handleConfirmButton();
      }
    }
  }
  lastJsButtonState = btnRaw;
}

void updateTimeSource() {
  // Feed the NMEA parser (non-blocking - no-op if GPS isn't wired in at all)
  gps.update();

  if (gps.hasFix()) {
    if (!gpsAvailable) {
      gpsAvailable = true;
      showStatusMessage(" GPS OK ");
    }

    // Periodically resync the RTC from GPS. If GPS is never wired in, hasFix()
    // just stays false forever and this block never runs - the RTC (or a
    // manual time set) is then the only time source, as intended.
    if (rtcAvailable && (lastGpsRtcSync == 0 || millis() - lastGpsRtcSync >= GPS_RTC_SYNC_INTERVAL)) {
      int year, month, day, dayIndex, hour, minute, second;
      gps.getHungarianTime(year, month, day, dayIndex, hour, minute, second);

      if (year >= SETTIME_MIN_YEAR) {  // sanity guard against a bad/partial fix
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
      }
      lastGpsRtcSync = millis();
    }
  } else if (gpsAvailable) {
    gpsAvailable = false;
  }

  // RTC is the single source of truth for the displayed time/date
  if (rtcAvailable && (millis() - lastRtcRead >= RTC_READ_INTERVAL)) {
    lastRtcRead = millis();

    DateTime now = rtc.now();
    if (now.isValid()) {
      currentTime.year = now.year();
      currentTime.month = now.month();
      currentTime.day = now.day();
      currentTime.dayIndex = now.dayOfTheWeek();
      currentTime.hour = now.hour();
      currentTime.minute = now.minute();
      currentTime.second = now.second();
    }
  }
}

void updateTDDisplay() {
  // The alarm's mode-independent tick (top of loop()) already owns the
  // display while it's ringing - don't let the normal per-mode logic below
  // race against it and flicker between "WAKE UP" and whatever mode is on
  // screen.
  if (alarmClock.isAlarmActive()) return;

  // Check if we're showing a status message
  if (showingStatusMessage) {
    if (millis() - statusMessageStart >= STATUS_MSG_DURATION) {
      showingStatusMessage = false;
      lastDisplayUpdate = millis();
    }
    return;
  }

  // Timer/Alarm drive their OWN internal timing (note steps, countdown ticks,
  // setting-title timeouts) via their own constants - they must be ticked every
  // call, not gated behind displayUpdateInterval, or their state machines stall
  // (this was the cause of the continuous drone / laggy value display).
  if (currentMode == 4) {
    timer.update();
    return;
  }
  if (currentMode == 5) {
    alarmClock.update();
    return;
  }

  // Regular display updates (only when not showing mode title)
  if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
    lastDisplayUpdate = millis();

    if ((currentMode == 0 || currentMode == 1 || currentMode == 2) && !rtcAvailable) {
      HDSP.displayText(" NO T&D ");
    } else {
      switch (currentMode) {
        case 0:
          HDSP.displayTime(currentTime.hour, currentTime.minute, currentTime.second, timeDisplayReversed);
          break;
        case 1:
          HDSP.displayYearMonth(currentTime.year, currentTime.month, dateDisplayReversed);
          break;
        case 2:
          HDSP.displayDayAndName(currentTime.day, currentTime.dayIndex, dayDisplayReversed);
          break;
        case 3:
          if (rtcAvailable) {
            HDSP.displayTemperature(currentTemperature, tempFahrenheit);
          } else {
            HDSP.displayText("NO TEMP ");
          }
          break;
        case 6:
          if (gpsAvailable) {
            HDSP.displayGPSSpeed(gps.getSpeedKmph());
          } else {
            HDSP.displayText(" NO GPS ");
          }
          break;
      }
    }
  }
}

void runDisplayTypeSetup() {
  pinMode(BUZZER, OUTPUT);
  pinMode(JS_SW, INPUT_PULLUP);

  uint8_t selectedType = preferences.getUChar("clockType", DEFAULT_CLOCK_TYPE);
  HDSP.setClockType(selectedType);
  HDSP.begin();
  HDSP.forceDisplayText(DISPLAY_TEST_TEXT);

  bool wasPressed = false;
  unsigned long pressStart = 0;
  byte pressCount = 0;
  unsigned long lastReleaseTime = 0;
  bool longPressHandled = false;

  while (true) {
    bool isPressed = (digitalRead(JS_SW) == LOW);

    // lenyomás (falling edge)
    if (isPressed && !wasPressed) {
      delay(DEBOUNCE_DELAY);
      if (digitalRead(JS_SW) != LOW) continue;  // zajszűrés
      wasPressed = true;
      pressStart = millis();
      longPressHandled = false;
    }

    // nyomva tartás - 2mp után confirm
    if (isPressed && wasPressed && !longPressHandled) {
      if (millis() - pressStart >= DISPLAY_SETUP_CONFIRM_HOLD) {
        longPressHandled = true;
        preferences.putUChar("clockType", selectedType);
        tone(BUZZER, 1500, 300);
        delay(300);
        return;  // confirmed - az óra normálisan indul tovább
      }
    }

    // elengedés (rising edge)
    if (!isPressed && wasPressed) {
      delay(DEBOUNCE_DELAY);
      if (digitalRead(JS_SW) == LOW) continue;  // zajszűrés
      wasPressed = false;

      if (!longPressHandled) {
        if (millis() - lastReleaseTime > DISPLAY_SETUP_PRESS_WINDOW) {
          pressCount = 0;  // túl nagy szünet, újrakezdjük a számlálást
        }
        pressCount++;
        lastReleaseTime = millis();
        tone(BUZZER, 800, 30);

        if (pressCount >= DISPLAY_SETUP_SWITCH_PRESSES) {
          selectedType = selectedType ? 0 : 1;
          HDSP.setClockType(selectedType);
          HDSP.begin();
          HDSP.forceDisplayText(DISPLAY_TEST_TEXT);
          pressCount = 0;
        }
      }
    }

    delay(5);
  }
}