#pragma once

#include "HDSPDisplay.h"
#include "constants.h"

// Manual RTC time setter. Triggered by triple UP-press from any normal mode.
// Cycles YEAR -> MNTH -> DAY -> HRS -> MINS. Seconds are NOT settable here -
// they're zeroed at the moment of the final CONFIRM, so you can line the
// press up with a real-world top-of-minute for a precise sync.
// This class only edits values in RAM; the main sketch reads them back via
// the getters and performs the actual rtc.adjust() (same ownership split as
// updateTimeSource() already uses - SetTime never touches the RTC object).
class SetTime {
private:
  byte currentSetting;  // 0=year, 1=month, 2=day, 3=hour, 4=minute

  int setYear;
  byte setMonth;
  byte setDay;
  byte setHour;
  byte setMinute;

  // Snapshot taken on reset(), restored by a single CANCEL press
  int startYear;
  byte startMonth;
  byte startDay;
  byte startHour;
  byte startMinute;

  bool showingSettingTitle;
  unsigned long settingTitleStartTime;

  unsigned long lastCancelPress;
  byte cancelPressCount;
  bool cancelSingleProcessed;

  bool exitSetTimeMode;
  bool commitTime;

  HDSPDisplay* display;
  byte buzzerPin;

  byte daysInMonthFor(byte month, int year) const {
    byte dim[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (month == 2 && isLeapYear) return 29;
    return dim[month - 1];
  }

  void clampDayToMonth() {
    byte maxDay = daysInMonthFor(setMonth, setYear);
    if (setDay > maxDay) setDay = maxDay;
  }

  // Single cancel press - revert current field to the value it had on entry
  void handleCancelSinglePress() {
    if (showingSettingTitle) return;

    switch (currentSetting) {
      case 0: setYear = startYear; clampDayToMonth(); break;
      case 1: setMonth = startMonth; clampDayToMonth(); break;
      case 2: setDay = startDay; break;
      case 3: setHour = startHour; break;
      case 4: setMinute = startMinute; break;
    }
  }

  String getCurrentSettingTitle() const {
    switch (currentSetting) {
      case 0: return "SET YEAR";
      case 1: return "SET MNTH";
      case 2: return "SET DAY ";
      case 3: return "SET HRS ";
      case 4: return "SET MINS";
      default: return "";
    }
  }

  void updateDisplay() {
    char buffer[9];

    if (showingSettingTitle) {
      getCurrentSettingTitle().toCharArray(buffer, 9);
    } else {
      switch (currentSetting) {
        case 0: sprintf(buffer, "YEAR%4d", setYear); break;
        case 1: sprintf(buffer, "MNTH  %02d", setMonth); break;
        case 2: sprintf(buffer, "DAY   %02d", setDay); break;
        case 3:
        case 4: sprintf(buffer, " %02d:%02d  ", setHour, setMinute); break;
      }
    }

    display->displayText(buffer);
  }

public:
  SetTime(HDSPDisplay* hdspDisplay, byte buzzer)
    : display(hdspDisplay), buzzerPin(buzzer) {
    exitSetTimeMode = false;
    commitTime = false;
  }

  // Call once when entering set-time mode; seeds fields from the currently known time
  void reset(int year, byte month, byte day, byte hour, byte minute) {
    currentSetting = 0;

    setYear = startYear = year;
    setMonth = startMonth = month;
    setDay = startDay = day;
    setHour = startHour = hour;
    setMinute = startMinute = minute;

    showingSettingTitle = true;
    settingTitleStartTime = millis();
    cancelPressCount = 0;
    lastCancelPress = 0;
    cancelSingleProcessed = false;
    exitSetTimeMode = false;
    commitTime = false;
  }

  void update() {
    if (cancelPressCount > 0 && (millis() - lastCancelPress > ALARM_CANCEL_DOUBLE_PRESS_WINDOW)) {
      if (cancelPressCount == 1 && !cancelSingleProcessed) {
        handleCancelSinglePress();
        cancelSingleProcessed = true;
      }
      cancelPressCount = 0;
    }

    if (showingSettingTitle && (millis() - settingTitleStartTime >= ALARM_SETTING_TITLE_DURATION)) {
      showingSettingTitle = false;
    }

    updateDisplay();
  }

  void handleConfirmButton() {
    if (showingSettingTitle) return;

    currentSetting++;
    if (currentSetting >= 5) {
      // All 5 fields set - commit (seconds zeroed at this exact instant)
      commitTime = true;
      exitSetTimeMode = true;
    } else {
      showingSettingTitle = true;
      settingTitleStartTime = millis();
    }
  }

  void handleAddButton() {
    if (showingSettingTitle) return;

    switch (currentSetting) {
      case 0:
        if (setYear < SETTIME_MAX_YEAR) setYear++;
        clampDayToMonth();
        break;
      case 1:
        setMonth = (setMonth >= 12) ? 1 : setMonth + 1;
        clampDayToMonth();
        break;
      case 2:
        setDay = (setDay >= daysInMonthFor(setMonth, setYear)) ? 1 : setDay + 1;
        break;
      case 3:
        setHour = (setHour >= 23) ? 0 : setHour + 1;
        break;
      case 4:
        setMinute = (setMinute >= 59) ? 0 : setMinute + 1;
        break;
    }
  }

  void handleSubtractButton() {
    if (showingSettingTitle) return;

    switch (currentSetting) {
      case 0:
        if (setYear > SETTIME_MIN_YEAR) setYear--;
        clampDayToMonth();
        break;
      case 1:
        setMonth = (setMonth <= 1) ? 12 : setMonth - 1;
        clampDayToMonth();
        break;
      case 2:
        setDay = (setDay <= 1) ? daysInMonthFor(setMonth, setYear) : setDay - 1;
        break;
      case 3:
        setHour = (setHour == 0) ? 23 : setHour - 1;
        break;
      case 4:
        setMinute = (setMinute == 0) ? 59 : setMinute - 1;
        break;
    }
  }

  void handleCancelButton() {
    unsigned long now = millis();

    // Double press -> abort the whole flow, nothing gets written to the RTC
    if (now - lastCancelPress <= ALARM_CANCEL_DOUBLE_PRESS_WINDOW && cancelPressCount == 1) {
      cancelPressCount = 0;
      commitTime = false;
      exitSetTimeMode = true;
      return;
    }

    cancelPressCount = 1;
    lastCancelPress = now;
    cancelSingleProcessed = false;
  }

  bool shouldExitSetTimeMode() const {
    return exitSetTimeMode;
  }
  bool shouldCommitTime() const {
    return commitTime;
  }
  void clearExitFlag() {
    exitSetTimeMode = false;
    commitTime = false;
  }

  int getYear() const {
    return setYear;
  }
  byte getMonth() const {
    return setMonth;
  }
  byte getDay() const {
    return setDay;
  }
  byte getHour() const {
    return setHour;
  }
  byte getMinute() const {
    return setMinute;
  }
};