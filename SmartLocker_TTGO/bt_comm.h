#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>

/**
 * TTGO acts as server but later should be client all connecting to the pi
 * The Raspberry Pi connects as a CLIENT using rfcomm for now until i figure something better.
 * 
 * Protocol:
 *   TTGO -> Pi:  "BORROW,{student_id}\n"     - Request to borrow
 *   TTGO -> Pi:  "RETURN,{student_id}\n"     - Notify return
 *   Pi -> TTGO:  "OK\n"                      - Authorization granted
 *   Pi -> TTGO:  "DENIED\n"                  - Authorization denied
 * 
 * if you want to hack it (theres absolutely no security here XD):
 */
class LockerBluetooth {
public:
  explicit LockerBluetooth(const char* deviceName = "SmartLockerTTGO");

  void begin();
  
  bool isConnected();
  
  bool sendBorrowRequest(const String &studentId);
  bool sendReturnNotification(const String &studentId);
  
  String readResponse();

private:
  BluetoothSerial _bt;
  String _deviceName;
  volatile bool _clientConnected;
  
  static LockerBluetooth* _instance;
  static void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
  
  bool sendMessage(const String &msg);
};
