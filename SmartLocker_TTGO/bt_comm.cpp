#include "bt_comm.h"

LockerBluetooth::LockerBluetooth(const char* deviceName)
  : _name(deviceName)
{
}

void LockerBluetooth::begin() {
  _bt.begin(_name);   // Starts Classic Bluetooth SPP server
  _rxBuffer.reserve(32);
}

bool LockerBluetooth::isConnected() {
  return _bt.hasClient();
}

bool LockerBluetooth::sendCode(const String &code) {
  if (!_bt.hasClient()) return false;
  _bt.print(code);
  _bt.print('\n');  // Non-blocking line send
  return true;
}

// Non-blocking response reading
String LockerBluetooth::readResponse() {
  // Read all available chars without blocking
  while (_bt.available()) {
    char c = (char)_bt.read();

    if (c == '\n') {
      // Completed message
      String msg = _rxBuffer;
      _rxBuffer = "";
      msg.trim();

      // Normalize
      msg.toUpperCase();

      if (msg == "OK")     return "OK";
      if (msg == "DENIED") return "DENIED";
      return msg; // Unexpected message
    }
    else {
      _rxBuffer += c;
    }
  }

  return "";  // No complete message yet
}
