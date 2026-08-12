#pragma once

// Clock kijelző típus (futásidőben állítható, NVS-ben tárolva "clockType" kulcs alatt)
// 0 = HDLY2416 (2x4 char, dupla CE1), 1 = HDSP2111 (1x8 char, közös WR/CE)
const uint8_t DEFAULT_CLOCK_TYPE = 0;

// Display típus setup / self-test képernyő
char *DISPLAY_TEST_TEXT = "Text OK?";                   // pontosan 8 karakter
const byte DISPLAY_SETUP_SWITCH_PRESSES = 5;            // ennyi rövid nyomás vált típust
const unsigned long DISPLAY_SETUP_PRESS_WINDOW = 1500;  // ms - ennyi időn belül kell jönniük egymás után
const unsigned long DISPLAY_SETUP_CONFIRM_HOLD = 2000;  // ms - ennyi ideig nyomva tartva confirmol

// MCP23017
const uint8_t MCP23017_ADDR = 0x20;  // A0/A1/A2 = GND

// Joystick
const byte JS_X = 0;
const byte JS_Y = 1;
const byte JS_SW = 2;

// Buzzer
const byte BUZZER = 9;
const int NOTIF_MELODY[] = { 523, 659, 783, 1047 };  // C5, E5, G5, C6
const int NOTIF_MELODY_LENGTH = 4;
const int STARTUP_MELODY[] = { 523, 659, 784, 1047, 1319, 1047 };  // C5 E5 G5 C6 E6 C6 - rise, peak, resolve
const int STARTUP_MELODY_LENGTH = 6;

// GPS
const byte GPS_TX = 3;
const byte GPS_RX = 4;
const uint16_t GPS_RATE_NORMAL_MS = 1000;  // 1 Hz - default GPS update rate
const uint16_t GPS_RATE_HIGH_MS = 200;     // 5 Hz - boosted while SPEED mode is on screen

// RTC
const byte I2C_SDA = 6;
const byte I2C_SCL = 7;

// Button debounce delay
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long JOYSTICK_REPEAT_INTERVAL = 300;

// Short day names based on day index
const char *DAYS_OF_WEEK[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// Intervals
const unsigned long RTC_READ_INTERVAL = 1000;          // 1 second
const unsigned long TEMPERATURE_READ_INTERVAL = 2000;  // 2 seconds
const unsigned long GPS_RTC_SYNC_INTERVAL = 60000;

// Mode titles
char *MODE_TITLES[] = {
  "HH:MM:SS",  // 0 - 14:00:00
  "YYYY. MM",  // 1 - 2025. 07
  " DD --- ",  // 2 - 10 Mon
  "- TEMP -",  // 3 - 25.6 C
  " TIMER  ",  // 4 - Timer mode
  " ALARM  ",  // 5 - Alarm mode
  "SPEED KM"   // 8 - 0 km/h
};

// Modes
const byte MIN_MODE = 0;
const byte MAX_MODE = 6;

// Durations
const int TITLE_SHOW_TIME = 2000;                // 2 seconds
const unsigned long STATUS_MSG_DURATION = 3000;  // 3 seconds
const unsigned long NOTIF_STEP_DURATION = 200;   // milliseconds
const int STARTUP_NOTE_DURATIONS[] = {
  90,   // C5 - quick
  90,   // E5 - quick
  90,   // G5 - quick
  160,  // C6 - arrival, slight hold
  240,  // E6 - peak / shimmer
  320   // C6 - resolve, held longest
};

// GPS timezone and time conversion constants
const unsigned long GPS_CACHE_VALIDITY_MS = 500;  // Cache valid for 500ms

// Timer state and timing constants
const unsigned long TIMER_COUNTDOWN_INTERVAL = 1000;         // 1 second
const unsigned long TIMER_SETTING_TITLE_DURATION = 2000;     // 2 seconds
const unsigned long TIMER_CANCEL_DOUBLE_PRESS_WINDOW = 500;  // 500ms for double press detection

// Timer + Alarm sound constants (shared - both ring with the same melody until
// the user interacts, per request; there's no cycle limit anymore)
const int TIMER_ALARM_MELODY[] = { 1200, 1400, 1600, 1400, 1600, 1800 };
const int TIMER_ALARM_MELODY_LENGTH = 6;
const unsigned long TIMER_ALARM_STEP_DURATION = 100;       // 100ms per note
const unsigned long TIMER_ALARM_CYCLE_GAP_DURATION = 300;  // 300ms gap between melody repeats

// Alarm constants
const unsigned long ALARM_SETTING_TITLE_DURATION = 2000;     // 2 seconds
const unsigned long ALARM_CANCEL_DOUBLE_PRESS_WINDOW = 500;  // 500ms for double press detection

// Button beep frequencies for unique press effects
const int BUTTON_BEEP_FREQS[] = { 800, 1000, 1200, 600 };  // CONFIRM, CANCEL, ADD, SUBTRACT
const int BUTTON_BEEP_DURATION = 50;

// Triple press detection (JOYSTICK UP x3) - enters manual time set mode
const unsigned long TRIPLE_PRESS_WINDOW = 3000;  // 3 seconds

// Manual time set (settime.h reuses ALARM_SETTING_TITLE_DURATION / ALARM_CANCEL_DOUBLE_PRESS_WINDOW above)
const int SETTIME_MIN_YEAR = 2024;
const int SETTIME_MAX_YEAR = 2099;