#pragma once
#include <Wire.h>
#include "constants.h"

#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_OLATA 0x14

// HDSP-2111: FL,A3,A4,CLS,RD 5V-re kötve (Character RAM szekció fix, belső osc, write-only)
// D7 GND-re (mindig ASCII kód), WR+CE közös vonal
#define HDSP_BIT_RST 0  // GPA0 - RST# (active low)
#define HDSP_BIT_A0 1   // GPA1
#define HDSP_BIT_A1 2   // GPA2
#define HDSP_BIT_A2 3   // GPA3
#define HDSP_BIT_WR 4   // GPA4 - WR# + CE# közös
#define HDSP_BIT_D0 5   // GPA5
#define HDSP_BIT_D1 6   // GPA6
#define HDSP_BIT_D2 7   // GPA7
#define HDSP_BIT_D3 0   // GPB0
#define HDSP_BIT_D4 1   // GPB1
#define HDSP_BIT_D5 2   // GPB2
#define HDSP_BIT_D6 3   // GPB3

#define HDLY_BIT_CE1_D1 0  // GPA0 - display1 CE1#
#define HDLY_BIT_CLR 1     // GPA1 - CLR#
#define HDLY_BIT_WR 2      // GPA2 - WR#
#define HDLY_BIT_A0 4      // GPA3
#define HDLY_BIT_A1 3      // GPA4
#define HDLY_BIT_CE1_D2 5  // GPA5 - display2 CE1#
#define HDLY_BIT_D0 6      // GPA6
#define HDLY_BIT_D1 7      // GPA7
#define HDLY_BIT_D2 0      // GPB0
#define HDLY_BIT_D3 1      // GPB1
#define HDLY_BIT_D4 4      // GPB2
#define HDLY_BIT_D5 3      // GPB3
#define HDLY_BIT_D6 2      // GPB4
#define HDLY_BIT_BL 5      // GPB5 - BL# (mindkét kijelző)

class HDSPDisplay {
private:
  uint8_t mcpAddr;
  uint8_t gpaState;
  uint8_t gpbState;
  char lastDisplayedText[9];
  bool displayInitialized;
  uint8_t clockType;  // 0 = HDLY2416, 1 = HDSP2111 - futásidőben váltható

  void writeRegisters() {
    Wire.beginTransmission(mcpAddr);
    Wire.write(MCP_OLATA);
    Wire.write(gpaState);
    Wire.write(gpbState);
    Wire.endTransmission();
  }

  void setGPA(uint8_t bit, bool val) {
    if (val) gpaState |= (1 << bit);
    else gpaState &= ~(1 << bit);
  }

  void setGPB(uint8_t bit, bool val) {
    if (val) gpbState |= (1 << bit);
    else gpbState &= ~(1 << bit);
  }

  // 'display' param csak HDLY2416-nál számít (0/1 = melyik chip), HDSP2111-nél figyelmen kívül marad
  void writeChar(uint8_t display, uint8_t pos, uint8_t code) {
    if (clockType == 1) {
      // ---- HDSP-2111 ----
      setGPA(HDSP_BIT_WR, true);
      setGPA(HDSP_BIT_A0, (pos >> 0) & 1);
      setGPA(HDSP_BIT_A1, (pos >> 1) & 1);
      setGPA(HDSP_BIT_A2, (pos >> 2) & 1);
      setGPA(HDSP_BIT_D0, (code >> 0) & 1);
      setGPA(HDSP_BIT_D1, (code >> 1) & 1);
      setGPA(HDSP_BIT_D2, (code >> 2) & 1);
      setGPB(HDSP_BIT_D3, (code >> 3) & 1);
      setGPB(HDSP_BIT_D4, (code >> 4) & 1);
      setGPB(HDSP_BIT_D5, (code >> 5) & 1);
      setGPB(HDSP_BIT_D6, (code >> 6) & 1);
      writeRegisters();
      delayMicroseconds(10);

      setGPA(HDSP_BIT_WR, false);  // WR#+CE# low - write active
      writeRegisters();
      delayMicroseconds(10);

      setGPA(HDSP_BIT_WR, true);  // latch + recovery
      writeRegisters();
      delayMicroseconds(10);
    } else {
      // ---- HDLY-2416 ----
      setGPA(HDLY_BIT_CE1_D1, true);
      setGPA(HDLY_BIT_CE1_D2, true);
      setGPA(HDLY_BIT_WR, true);
      setGPA(HDLY_BIT_A0, (pos >> 0) & 1);
      setGPA(HDLY_BIT_A1, (pos >> 1) & 1);
      setGPA(HDLY_BIT_D0, (code >> 0) & 1);
      setGPA(HDLY_BIT_D1, (code >> 1) & 1);
      setGPB(HDLY_BIT_D2, (code >> 2) & 1);
      setGPB(HDLY_BIT_D3, (code >> 3) & 1);
      setGPB(HDLY_BIT_D4, (code >> 4) & 1);
      setGPB(HDLY_BIT_D5, (code >> 5) & 1);
      setGPB(HDLY_BIT_D6, (code >> 6) & 1);
      writeRegisters();
      delayMicroseconds(10);

      if (display == 0) setGPA(HDLY_BIT_CE1_D1, false);
      else setGPA(HDLY_BIT_CE1_D2, false);
      writeRegisters();
      delayMicroseconds(10);

      setGPA(HDLY_BIT_WR, false);
      writeRegisters();
      delayMicroseconds(10);

      setGPA(HDLY_BIT_WR, true);
      writeRegisters();
      delayMicroseconds(10);

      setGPA(HDLY_BIT_CE1_D1, true);
      setGPA(HDLY_BIT_CE1_D2, true);
      writeRegisters();
      delayMicroseconds(10);
    }
  }

  void sendToDisplay(char* data) {
    if (clockType == 1) {
      for (int i = 0; i < 8; i++) {
        char ch = (data[i] != '\0') ? data[i] : ' ';
        writeChar(0, i, (uint8_t)ch);  // DIG0..DIG7 = addr 0..7, bal->jobb, nincs tükrözés
      }
    } else {
      for (int i = 0; i < 8; i++) {
        char ch = (data[i] != '\0') ? data[i] : ' ';
        uint8_t disp = (i < 4) ? 0 : 1;
        uint8_t addr = (disp == 0) ? (3 - i) : (7 - i);
        writeChar(disp, addr, (uint8_t)ch);
      }
    }
  }

public:
  HDSPDisplay(uint8_t addr = MCP23017_ADDR, uint8_t type = 0)
    : mcpAddr(addr), gpaState(0xFF), gpbState(0x20), displayInitialized(false), clockType(type) {
    for (int i = 0; i < 9; i++) lastDisplayedText[i] = '\0';
  }

  void setClockType(uint8_t type) {
    clockType = type;
  }

  void begin() {
    Wire.beginTransmission(mcpAddr);
    Wire.write(MCP_IODIRA);
    Wire.write(0x00);  // GPA all output
    Wire.write(0x00);  // GPB all output
    Wire.endTransmission();

    if (clockType == 1) {
      gpaState = 0x11;  // RST=1, WR/CE=1 (idle)
      gpbState = 0x00;
    } else {
      gpaState = 0xFF;
      gpbState = 0x20;  // BL# HIGH
    }
    writeRegisters();
    displayInitialized = false;
    delay(50);
  }

  void resetDisplay() {
    if (clockType == 1) {
      setGPA(HDSP_BIT_RST, false);  // RST# low - reset assert
      writeRegisters();
      delay(10);

      setGPA(HDSP_BIT_RST, true);  // RST# high - release (>=110us kell, 10ms bőven elég)
      writeRegisters();
      delay(10);
    } else {
      setGPA(HDLY_BIT_CE1_D1, false);
      setGPA(HDLY_BIT_CE1_D2, false);
      setGPA(HDLY_BIT_CLR, false);
      writeRegisters();
      delay(10);

      setGPA(HDLY_BIT_CLR, true);
      writeRegisters();
      delay(10);

      setGPA(HDLY_BIT_CE1_D1, true);
      setGPA(HDLY_BIT_CE1_D2, true);
      writeRegisters();
      delay(10);
    }

    for (int i = 0; i < 9; i++) lastDisplayedText[i] = '\0';
    displayInitialized = true;
  }

  void displayText(char* text) {
    char buffer[9];
    bool endReached = false;
    for (int i = 0; i < 8; i++) {
      if (!endReached && text[i] == '\0') endReached = true;
      buffer[i] = endReached ? ' ' : text[i];
    }
    buffer[8] = '\0';

    bool contentChanged = !displayInitialized;
    if (!contentChanged) {
      for (int i = 0; i < 8; i++) {
        if (buffer[i] != lastDisplayedText[i]) {
          contentChanged = true;
          break;
        }
      }
    }

    if (contentChanged) {
      if (!displayInitialized) resetDisplay();
      sendToDisplay(buffer);
      for (int i = 0; i < 9; i++) lastDisplayedText[i] = buffer[i];
    }
  }

  void displayTime(byte hour, byte minute, byte second, bool reversed = false) {
    char buffer[9];
    if (reversed) sprintf(buffer, "%02d:%02d:%02d", second, minute, hour);
    else sprintf(buffer, "%02d:%02d:%02d", hour, minute, second);
    displayText(buffer);
  }

  void displayYearMonth(int year, byte month, bool reversed = false) {
    char buffer[9];
    if (reversed) sprintf(buffer, "%02d. %d", month, year);
    else sprintf(buffer, "%d. %02d", year, month);
    displayText(buffer);
  }

  void displayDayAndName(byte day, byte dayIndex, bool reversed = false) {
    char buffer[9];
    if (reversed) sprintf(buffer, " %s %02d ", DAYS_OF_WEEK[dayIndex], day);
    else sprintf(buffer, " %02d %s ", day, DAYS_OF_WEEK[dayIndex]);
    displayText(buffer);
  }

  void displayTemperature(float temperature, bool fahrenheit = false) {
    char buffer[16];
    float t = fahrenheit ? (temperature * 9.0f / 5.0f + 32.0f) : temperature;
    char unit = fahrenheit ? 'F' : 'C';
    if (t >= 100.0 || t <= -100.0)
      sprintf(buffer, " %03.0f %c ", abs((int)t), unit);
    else if (t >= 10.0 || t <= -10.0)
      sprintf(buffer, " %04.1f %c ", abs(t), unit);
    else if (t >= 0.0)
      sprintf(buffer, " %04.2f %c ", t, unit);
    else
      sprintf(buffer, " -%03.1f %c ", abs(t), unit);
    displayText(buffer);
  }

  void displayGPSSpeed(double speed) {
    char buffer[16];
    if (speed >= 100.0) sprintf(buffer, "%03.0f km/h ", speed);
    else if (speed >= 10.0) sprintf(buffer, " %02.0f km/h ", speed);
    else sprintf(buffer, " %01.0f km/h ", speed);
    displayText(buffer);
  }

  void forceDisplayText(char* text) {
    resetDisplay();
    displayText(text);
  }
};