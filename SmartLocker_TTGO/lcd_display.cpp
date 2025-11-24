#include "lcd_display.h"
#include <Wire.h>
#include "config.h"

LCDDisplay::LCDDisplay(uint8_t addr,
                       uint8_t cols,
                       uint8_t rows)
  : _cols(cols),
    _rows(rows),
    _lcd(addr, cols, rows)
{
}

void LCDDisplay::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  _lcd.init();
  _lcd.backlight();
  _lcd.clear();
}

void LCDDisplay::clear() {
  _lcd.clear();
}

void LCDDisplay::printLines(const String &line1,
                            const String &line2) {
  // Line 0
  _lcd.setCursor(0, 0);
  String l1 = line1;
  if (l1.length() > _cols) {
    l1 = l1.substring(0, _cols);   // cut if too long
  }
  _lcd.print(l1);
  for (int i = l1.length(); i < _cols; ++i) {
    _lcd.print(' ');               // pad to erase old chars
  }

  // Line 1
  if (_rows > 1) {
    _lcd.setCursor(0, 1);
    String l2 = line2;
    if (l2.length() > _cols) {
      l2 = l2.substring(0, _cols);
    }
    _lcd.print(l2);
    for (int i = l2.length(); i < _cols; ++i) {
      _lcd.print(' ');
    }
  }
}
