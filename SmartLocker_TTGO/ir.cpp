#include "ir.h"
#include "config.h"

// PArent
IRSensor::IRSensor(int pin, int threshold)
  : _pin(pin),
    _threshold(threshold)
{
}

void IRSensor::begin() {
  pinMode(_pin, INPUT);
}

int IRSensor::readRaw() {
  return analogRead(_pin);
}

bool IRSensor::isTriggered() {
  return readRaw() >= _threshold;
}


BoxIR::BoxIR()
  : IRSensor(PIN_IR_SENSOR_BOX, IR_BOX_THRESHOLD)
{
}

DoorIR::DoorIR()
  : IRSensor(PIN_IR_SENSOR_DOOR, IR_DOOR_THRESHOLD)
{
}