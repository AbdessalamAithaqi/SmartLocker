#pragma once

#include <Arduino.h>

class IRSensor {
public:
  IRSensor(int pin, int threshold);

  void begin();

  int  readRaw();
  bool isTriggered();

protected:
  int _pin;
  int _threshold;
};

class BoxIR : public IRSensor {
public:
  BoxIR();

  void begin() { IRSensor::begin(); }

  int  readRaw()      { return IRSensor::readRaw(); }
  bool isBoxPresent() { return isTriggered(); }
};

class DoorIR : public IRSensor {
public:
  DoorIR();

  void begin() { IRSensor::begin(); }

  int  readRaw()       { return IRSensor::readRaw(); }
  bool isDoorClosed()  { return isTriggered(); }
};
