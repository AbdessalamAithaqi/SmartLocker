#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>

/**
 * LockerBluetooth - Bluetooth SPP Server for SmartLocker
 * 
 * The TTGO acts as a Bluetooth SERVER.
 * The Raspberry Pi connects as a CLIENT using rfcomm.
 * 
 * Protocol:
 *   TTGO -> Pi:  "BORROW,{student_id}\n"     - Request to borrow
 *   TTGO -> Pi:  "RETURN,{student_id}\n"     - Notify return
 *   Pi -> TTGO:  "OK\n"                      - Authorization granted
 *   Pi -> TTGO:  "DENIED\n"                  - Authorization denied
 */
class LockerBluetooth {
public:
  explicit LockerBluetooth(const char* deviceName = "SmartLockerTTGO");

  void begin();
  
  // Connection status
  bool isConnected();
  
  // Communication - send commands to Pi
  bool sendBorrowRequest(const String &studentId);
  bool sendReturnNotification(const String &studentId);
  
  // Read response from Pi (non-blocking)
  // Returns: "OK", "DENIED", or "" (no message yet)
  String readResponse();

private:
  BluetoothSerial _bt;
  String _deviceName;
  volatile bool _clientConnected;
  
  // SPP callback needs access to _clientConnected
  static LockerBluetooth* _instance;
  static void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
  
  bool sendMessage(const String &msg);
};
