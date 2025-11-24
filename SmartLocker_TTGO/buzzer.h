#pragma once

#include <Arduino.h>

class Buzzer {
public:
  Buzzer();

  void begin();

  void on();
  void off();

private:
  uint8_t _pin;
};
