#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

class LCDDisplay {
public:
  // Defaults of the LCD screen i have: addr=0x27, 16x2
  explicit LCDDisplay(uint8_t addr = 0x27, uint8_t cols = 16, uint8_t rows = 2);

  void begin();

  void clear();

  void printLines(const String &line1,
                  const String &line2 = "");

private:
  uint8_t _cols;
  uint8_t _rows;
  LiquidCrystal_I2C _lcd;
};
