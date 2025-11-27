#include "servo.h"
#include "config.h"

DoorServo::DoorServo() {
}

void DoorServo::begin() {
  _servo.setPeriodHertz(50);                      
  _servo.attach(PIN_SERVO_LOCK, 500, 2500);      
}

void DoorServo::lock() {
  _servo.write(DOOR_LOCKED_ANGLE);
}

void DoorServo::unlock() {
  _servo.write(DOOR_UNLOCKED_ANGLE);
}
