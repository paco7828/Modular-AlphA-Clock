#pragma once

#include "HDSPDisplay.h"
#include "constants.h"

class Timer {
private:
  // Timer state
  bool settingTimer;
  bool startedTimer;
  bool timerFinished;
  byte currentSetting;  // 0=hours, 1=minutes, 2=seconds

  // Timer values
  int currentHours;
  int currentMinutes;
  int currentSeconds;

  // Countdown tracking
  unsigned long previousMillis;

  // Timer alarm - FIXED timing variables
  bool alarmPlaying;
  unsigned long alarmStartTime;
  byte alarmStep;
  unsigned long alarmCycleGap;
  unsigned long noteStartTime;  // Track individual note timing

  // Setting display state
  bool showingSettingTitle;
  unsigned long settingTitleStartTime;

  // Cancel button tracking - UPDATED for single/double press detection
  unsigned long lastCancelPress;
  byte cancelPressCount;
  bool cancelSingleProcessed;

  // Reference to external components
  HDSPDisplay* display;
  byte buzzerPin;

public:
  Timer(HDSPDisplay* hdspDisplay, byte buzzer)
    : display(hdspDisplay), buzzerPin(buzzer) {
    reset();
  }

  void reset() {
    settingTimer = true;
    startedTimer = false;
    timerFinished = false;
    currentSetting = 0;
    currentHours = 0;
    currentMinutes = 0;
    currentSeconds = 0;
    previousMillis = 0;
    alarmPlaying = false;
    alarmStartTime = 0;
    alarmStep = 0;
    alarmCycleGap = 0;
    noteStartTime = 0;  // Initialize note timing
    showingSettingTitle = true;
    settingTitleStartTime = millis();
    cancelPressCount = 0;
    lastCancelPress = 0;
    cancelSingleProcessed = false;
    exitTimerMode = false;
  }

  // Handle timer updates (call this in main loop when in timer mode)
  void update() {
    // Handle cancel button timeout and single press processing
    if (cancelPressCount > 0 && (millis() - lastCancelPress > TIMER_CANCEL_DOUBLE_PRESS_WINDOW)) {
      if (cancelPressCount == 1 && !cancelSingleProcessed) {
        // Process single press - reset current setting to 0
        handleCancelSinglePress();
        cancelSingleProcessed = true;
      }
      cancelPressCount = 0;
    }

    // Handle setting title timeout
    if (showingSettingTitle && (millis() - settingTitleStartTime >= TIMER_SETTING_TITLE_DURATION)) {
      showingSettingTitle = false;
    }

    if (alarmPlaying) {
      handleAlarm();
      updateAlarmDisplay();
      return;
    }

    if (startedTimer) {
      handleCountdown();
    }

    updateDisplay();
  }

  // Force show setting title (call this when entering timer mode)
  void forceShowSettingTitle() {
    showingSettingTitle = true;
    settingTitleStartTime = millis();
  }

  // Button handlers
  void handleConfirmButton() {
    if (alarmPlaying || timerFinished) {
      // Stop alarm and reset
      stopAlarm();
      reset();
      return;
    }

    if (settingTimer) {
      handleValueConfirm();
    } else if (!startedTimer) {
      handleTimerStart();
    } else {
      handleTimerStop();
    }
  }

  void handleAddButton() {
    if (alarmPlaying || timerFinished) {
      stopAlarm();
      reset();
      return;
    }
    if (settingTimer && !alarmPlaying && !showingSettingTitle) {
      handleAdd();
    }
  }

  void handleSubtractButton() {
    if (alarmPlaying || timerFinished) {
      stopAlarm();
      reset();
      return;
    }
    if (settingTimer && !alarmPlaying && !showingSettingTitle) {
      handleSubtract();
    }
  }

  void handleCancelButton() {
    if (alarmPlaying || timerFinished) {
      stopAlarm();
      reset();
      exitTimerMode = true;
      return;
    }

    unsigned long currentTime = millis();

    // Check for double press
    if (currentTime - lastCancelPress <= TIMER_CANCEL_DOUBLE_PRESS_WINDOW && cancelPressCount == 1) {
      // Double press detected - exit timer mode
      cancelPressCount = 0;
      exitTimerMode = true;
      return;
    }

    // Record the press
    cancelPressCount = 1;
    lastCancelPress = currentTime;
    cancelSingleProcessed = false;  // Reset flag for new press
  }

  // Get current state for display purposes
  bool shouldExitTimerMode() const {
    return exitTimerMode;
  }

  // Reset exit flag (call from main code after handling exit)
  void clearExitFlag() {
    exitTimerMode = false;
  }

private:
  bool exitTimerMode = false;  // Flag for double-cancel exit

  // NEW: Handle single cancel press - reset current setting to 0
  void handleCancelSinglePress() {
    if (!settingTimer || showingSettingTitle) return;  // Only work in setting mode, not during title display

    switch (currentSetting) {
      case 0:  // Hours
        currentHours = 0;
        break;
      case 1:  // Minutes
        currentMinutes = 0;
        break;
      case 2:  // Seconds
        currentSeconds = 0;
        break;
    }
  }

  // Format timer values with leading zeros
  String formatTimerValue(int value) const {
    if (value < 10) {
      return "0" + String(value);
    }
    return String(value);
  }

  // Get formatted timer string
  String getTimerString() const {
    return formatTimerValue(currentHours) + ":" + formatTimerValue(currentMinutes) + ":" + formatTimerValue(currentSeconds);
  }

  // Get current setting title
  String getCurrentSettingTitle() const {
    switch (currentSetting) {
      case 0: return "SET HRS ";
      case 1: return "SET MIN ";
      case 2: return "SET SEC ";
      default: return "";
    }
  }

  // Handle value confirmation (hours -> minutes -> seconds -> done)
  void handleValueConfirm() {
    if (showingSettingTitle) return;  // Ignore if still showing title

    currentSetting++;
    if (currentSetting >= 3) {
      // All values set, check if timer has valid time
      if (currentHours == 0 && currentMinutes == 0 && currentSeconds == 0) {
        // No time set, reset to beginning
        currentSetting = 0;
        showingSettingTitle = true;
        settingTitleStartTime = millis();
      } else {
        // Valid time set, exit setting mode
        settingTimer = false;
      }
    } else {
      // Move to next setting, show title
      showingSettingTitle = true;
      settingTitleStartTime = millis();
    }
  }

  // Handle adding to current setting
  void handleAdd() {
    switch (currentSetting) {
      case 0:  // Hours
        currentHours++;
        if (currentHours > 23) {
          currentHours = 0;
        }
        break;
      case 1:  // Minutes
        currentMinutes++;
        if (currentMinutes > 59) {
          currentMinutes = 0;
        }
        break;
      case 2:  // Seconds
        currentSeconds++;
        if (currentSeconds > 59) {
          currentSeconds = 0;
        }
        break;
    }
  }

  // Handle subtracting from current setting
  void handleSubtract() {
    switch (currentSetting) {
      case 0:  // Hours
        currentHours--;
        if (currentHours < 0) {
          currentHours = 23;
        }
        break;
      case 1:  // Minutes
        currentMinutes--;
        if (currentMinutes < 0) {
          currentMinutes = 59;
        }
        break;
      case 2:  // Seconds
        currentSeconds--;
        if (currentSeconds < 0) {
          currentSeconds = 59;
        }
        break;
    }
  }

  // Start the timer countdown
  void handleTimerStart() {
    if (currentHours == 0 && currentMinutes == 0 && currentSeconds == 0) {
      // Can't start with 0 time, go back to setting
      settingTimer = true;
      currentSetting = 0;
      showingSettingTitle = true;
      settingTitleStartTime = millis();
      return;
    }

    startedTimer = true;
    previousMillis = millis();
  }

  // Stop/pause the timer
  void handleTimerStop() {
    startedTimer = false;
  }

  // Handle countdown logic
  void handleCountdown() {
    unsigned long currentMillis = millis();

    // Every 1 second
    if (currentMillis - previousMillis >= TIMER_COUNTDOWN_INTERVAL) {
      previousMillis = currentMillis;

      // Countdown logic
      if (currentSeconds > 0) {
        currentSeconds--;
      } else if (currentMinutes > 0) {
        currentMinutes--;
        currentSeconds = 59;
      } else if (currentHours > 0) {
        currentHours--;
        currentMinutes = 59;
        currentSeconds = 59;
      } else {
        // Timer finished!
        startedTimer = false;
        timerFinished = true;
        startAlarm();
      }
    }
  }

  // Start the timer alarm - loops until stopped by any joystick interaction
  void startAlarm() {
    alarmPlaying = true;
    alarmStartTime = millis();
    noteStartTime = millis();  // Initialize note timing
    alarmStep = 0;
    alarmCycleGap = 0;

    // Stop any existing tone and play first tone WITHOUT duration parameter
    noTone(buzzerPin);
    delay(10);
    tone(buzzerPin, TIMER_ALARM_MELODY[0]);  // NO duration parameter!
  }

  // Handle alarm progression - loops the melody forever (gap between passes)
  // until the user interacts; there's no cycle limit / auto-stop anymore.
  void handleAlarm() {
    unsigned long currentMillis = millis();

    // Handle gap between melody repeats
    if (alarmCycleGap > 0) {
      if (currentMillis - alarmCycleGap >= TIMER_ALARM_CYCLE_GAP_DURATION) {
        alarmCycleGap = 0;
        alarmStep = 0;
        noteStartTime = currentMillis;

        noTone(buzzerPin);
        delay(5);
        tone(buzzerPin, TIMER_ALARM_MELODY[0]);
      }
      return;
    }

    // Check if current note duration has elapsed
    if (currentMillis - noteStartTime >= TIMER_ALARM_STEP_DURATION) {
      alarmStep++;

      if (alarmStep < TIMER_ALARM_MELODY_LENGTH) {
        // Play next tone in melody
        noteStartTime = currentMillis;
        noTone(buzzerPin);
        delay(5);
        tone(buzzerPin, TIMER_ALARM_MELODY[alarmStep]);
      } else {
        // Melody finished - brief gap, then repeat (forever, until stopped)
        noTone(buzzerPin);
        alarmCycleGap = currentMillis;
      }
    }
  }

  // Stop the alarm
  void stopAlarm() {
    alarmPlaying = false;
    timerFinished = false;
    noTone(buzzerPin);
  }

  // Update display based on current state
  void updateDisplay() {
    if (settingTimer) {
      if (showingSettingTitle) {
        // Show setting title (SET HRS, SET MIN, SET SEC)
        String titleStr = getCurrentSettingTitle();
        char buffer[9];
        titleStr.toCharArray(buffer, 9);
        display->displayText(buffer);
      } else {
        // Show current timer values
        String timerStr = getTimerString();
        char buffer[9];
        timerStr.toCharArray(buffer, 9);
        display->displayText(buffer);
      }
    } else {
      // Show countdown or ready timer
      String timerStr = getTimerString();
      char buffer[9];
      timerStr.toCharArray(buffer, 9);
      display->displayText(buffer);
    }
  }

  // Update alarm display
  void updateAlarmDisplay() {
    // Flash "TIME UP" message
    if ((millis() / 300) % 2 == 0) {
      display->displayText("TIME UP ");
    } else {
      display->displayText("        ");
    }
  }
};