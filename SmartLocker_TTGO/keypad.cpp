#include "keypad.h"
#include "config.h"

static const byte ROWS = 4;
static const byte COLS = 4;

static char s_keymap[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

static byte s_rowPins[ROWS] = {
  PIN_KEYPAD_R0,
  PIN_KEYPAD_R1,
  PIN_KEYPAD_R2,
  PIN_KEYPAD_R3
};

static byte s_colPins[COLS] = {
  PIN_KEYPAD_C0,
  PIN_KEYPAD_C1,
  PIN_KEYPAD_C2,
  PIN_KEYPAD_C3
};

LockerKeypad::LockerKeypad()
  : _keypad(makeKeymap(s_keymap), s_rowPins, s_colPins, ROWS, COLS)
{
}

void LockerKeypad::begin() {
}

char LockerKeypad::getKey() {
  char k = _keypad.getKey();
  return k ? k : 0;
}

bool LockerKeypad::getKey(char &outKey) {
  char k = _keypad.getKey();
  if (!k) {
    return false;
  }
  outKey = k;
  return true;
}
