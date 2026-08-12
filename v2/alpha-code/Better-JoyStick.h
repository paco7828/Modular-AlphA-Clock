class BetterJoystick {
private:
  const int X_CENTER = 3250;
  const int Y_CENTER = 3500;
  const int DEADZONE = 100;

  byte JS_SW = 0;
  byte JS_VRX = 0;
  byte JS_VRY = 0;

  // Function to get more accurate mapped values
  int mapJoystick(int value, int center) {
    if (abs(value - center) < DEADZONE) return 0;

    int mappedValue;
    if (value > center) {
      mappedValue = map(value, center + DEADZONE, 4095, 0, 100);
      if (mappedValue > 90) return 100;  // snap to 100
      return mappedValue;
    } else {
      mappedValue = map(value, 0, center - DEADZONE, -100, 0);
      if (mappedValue < -90) return -100;  // snap to -100
      return mappedValue;
    }
  }


public:
  void begin(byte SW, byte VRX, byte VRY) {
    JS_SW = SW;
    JS_VRX = VRX;
    JS_VRY = VRY;
    pinMode(JS_SW, INPUT_PULLUP);
  }

  // Raw analog values
  int getRawX() {
    return analogRead(JS_VRX);
  }
  int getRawY() {
    return analogRead(JS_VRY);
  }

  // Mapped values
  int getMappedX() {
    return mapJoystick(getRawX(), X_CENTER);
  }
  int getMappedY() {
    return mapJoystick(getRawY(), Y_CENTER);
  }

  // Button press
  bool getButtonPress() {
    return digitalRead(JS_SW) == LOW;
  }

  // Get direction (1-8) or 0 if centered
  // Get direction (1-8) or 0 if centered
  byte getDirection() {
    int x = getMappedX();
    int y = getMappedY();

    // Diagonals first
    if (x == 100 && y == 100) return 5;    // TOP RIGHT
    if (x == -100 && y == 100) return 6;   // TOP LEFT
    if (x == 100 && y == -100) return 7;   // BOTTOM RIGHT
    if (x == -100 && y == -100) return 8;  // BOTTOM LEFT

    // Single directions
    if (x == -100) return 1;  // LEFT
    if (x == 100) return 2;   // RIGHT
    if (y == -100) return 3;  // BOTTOM
    if (y == 100) return 4;   // TOP

    return 0;  // Centered / no direction
  }
};
