#pragma once

#include <Arduino.h>
#include <Keypad.h>

class LockerKeypad {
public:
  LockerKeypad();

  void begin();

  char getKey();

  bool getKey(char &outKey);

private:
  static const byte ROWS = 4;
  static const byte COLS = 4;

  Keypad _keypad;
};
