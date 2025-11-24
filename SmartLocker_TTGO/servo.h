#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

class DoorServo {
public:
  DoorServo();

  void begin();

  void lock();
  void unlock();

private:
  Servo _servo;
};
