#pragma once

#include <Arduino.h>
#include <Keypad.h>

class LockerKeypad {
public:
  LockerKeypad();

  void begin();

  // returns 0 if no key, otherwise the char ('0'..'9', 'A'..'D', '*', '#')
  char getKey();

  // Convenience overload: returns true if a key was read, stores it in outKey
  bool getKey(char &outKey);

private:
  static const byte ROWS = 4;
  static const byte COLS = 4;

  Keypad _keypad;
};
