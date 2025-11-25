#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>

class LockerBluetooth {
public:
  explicit LockerBluetooth(const char* serverAddress);

  void begin();
  
  // Connection management
  bool connect();              // Connect to server
  void disconnect();           // Disconnect from server
  bool isConnected();          // Check if connected
  
  // Communication
  bool sendCode(const String &code);
  String readResponse();

private:
  BluetoothSerial _bt;
  String _serverAddress;       // MAC address of Pi server
  String _rxBuffer;
  bool _connected;
  unsigned long _lastConnectAttempt;
};