#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>

class LockerBluetooth {
public:
  explicit LockerBluetooth(const char* deviceName = "LockerNode");

  void begin();

  bool isConnected();

  bool sendCode(const String &code);

  String readResponse();

private:
  BluetoothSerial _bt;
  const char* _name;

  String _rxBuffer;
};
