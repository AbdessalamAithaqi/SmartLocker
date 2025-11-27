#include "led.h"
#include "config.h"

LED::LED(uint8_t pin)
  : _pin(pin)
{
}

void LED::begin() {
  pinMode(_pin, OUTPUT);
  off();  // start off
}

void LED::on() {
  digitalWrite(_pin, HIGH);
}

void LED::off() {
  digitalWrite(_pin, LOW);
}

GreenLED::GreenLED()
  : LED(PIN_LED_GREEN)
{
}
