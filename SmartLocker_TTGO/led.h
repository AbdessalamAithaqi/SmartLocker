#pragma once

#include <Arduino.h>

class LED {
public:
  explicit LED(uint8_t pin);

  void begin();

  void on();
  void off();

protected:
  uint8_t _pin;
};

class GreenLED : public LED {
public:
  GreenLED();
};

class RedLED : public LED {
public:
  RedLED();
};
