#include "bt_comm.h"

LockerBluetooth::LockerBluetooth(const char* serverAddress)
  : _serverAddress(serverAddress),
    _connected(false),
    _lastConnectAttempt(0)
{
}

void LockerBluetooth::begin() {
  _bt.begin("SmartLocker_Client", false);  // Start as CLIENT (not master)
  _rxBuffer.reserve(32);
  Serial.println("Bluetooth client initialized");
}

bool LockerBluetooth::connect() {
  // Prevent connection spam - only try every 5 seconds
  if (millis() - _lastConnectAttempt < 5000) {
    return _connected;
  }
  
  _lastConnectAttempt = millis();
  
  if (_connected) {
    return true;  // Already connected
  }
  
  Serial.print("Attempting to connect to Pi: ");
  Serial.println(_serverAddress);
  
  // Connect to the Pi server
  _connected = _bt.connect(_serverAddress);
  
  if (_connected) {
    Serial.println("✓ Connected to Pi server");
    _rxBuffer = "";  // Clear buffer
  } else {
    Serial.println("✗ Connection failed");
  }
  
  return _connected;
}

void LockerBluetooth::disconnect() {
  if (_connected) {
    _bt.disconnect();
    _connected = false;
    Serial.println("Disconnected from Pi");
  }
}

bool LockerBluetooth::isConnected() {
  // Check actual connection status
  if (!_bt.connected()) {
    _connected = false;
  }
  return _connected;
}

bool LockerBluetooth::sendCode(const String &code) {
  if (!_connected || !_bt.connected()) {
    Serial.println("Not connected - cannot send");
    _connected = false;
    return false;
  }
  
  // Send the code with newline
  _bt.print(code);
  _bt.print('\n');
  
  Serial.print("Sent to Pi: ");
  Serial.println(code);
  
  return true;
}

String LockerBluetooth::readResponse() {
  if (!_connected || !_bt.connected()) {
    _connected = false;
    return "";
  }
  
  // Read all available chars without blocking
  while (_bt.available()) {
    char c = (char)_bt.read();

    if (c == '\n') {
      // Completed message
      String msg = _rxBuffer;
      _rxBuffer = "";
      msg.trim();
      msg.toUpperCase();
      
      Serial.print("Received from Pi: ");
      Serial.println(msg);

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