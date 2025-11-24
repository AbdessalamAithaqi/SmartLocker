#include "buzzer.h"
#include "config.h"

static const uint8_t BUZZER_PIN = PIN_BUZZER;

Buzzer::Buzzer()
  : _pin(BUZZER_PIN)
{
}

void Buzzer::begin() {
  pinMode(_pin, OUTPUT);
  off();
}

void Buzzer::on() {
  digitalWrite(_pin, HIGH);
}

void Buzzer::off() {
  digitalWrite(_pin, LOW);
}
