#include "bt_comm.h"
#include "esp_spp_api.h"   // for SPP callback types

// Static instance pointer for callback
LockerBluetooth* LockerBluetooth::_instance = nullptr;

// Static callback function for SPP events (same pattern as working code)
void LockerBluetooth::btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (_instance == nullptr) return;
  
  switch (event) {
    case ESP_SPP_SRV_OPEN_EVT:   // Client (Pi) connected
      _instance->_clientConnected = true;
      Serial.println("[BT] Pi connected!");
      break;

    case ESP_SPP_CLOSE_EVT:      // Client disconnected
      _instance->_clientConnected = false;
      Serial.println("[BT] Pi disconnected");
      break;

    default:
      break;
  }
}

LockerBluetooth::LockerBluetooth(const char* deviceName)
  : _deviceName(deviceName),
    _clientConnected(false)
{
  _instance = this;
}

void LockerBluetooth::begin() {
  // Register callback before starting (same as working code)
  _bt.register_callback(btCallback);
  
  // Start as Bluetooth SPP SERVER
  // The Pi will connect to us using rfcomm
  if (!_bt.begin(_deviceName)) {
    Serial.println("[BT] ERROR: Failed to start Bluetooth!");
    while (true) {
      delay(1000);  // Fatal error, hang
    }
  }
  
  Serial.print("[BT] Server started as '");
  Serial.print(_deviceName);
  Serial.println("'");
  Serial.println("[BT] Waiting for Pi to connect...");
  Serial.println("[BT] (Use 'bluetoothctl' on Pi to find MAC address)");
}

bool LockerBluetooth::isConnected() {
  return _clientConnected && _bt.connected();
}

bool LockerBluetooth::sendMessage(const String &msg) {
  if (!isConnected()) {
    Serial.println("[BT] Cannot send - Pi not connected");
    return false;
  }
  
  _bt.print(msg);
  _bt.print('\n');
  _bt.flush();
  
  Serial.print("[BT] Sent: ");
  Serial.println(msg);
  
  return true;
}

bool LockerBluetooth::sendBorrowRequest(const String &studentId) {
  String msg = "BORROW," + studentId;
  return sendMessage(msg);
}

bool LockerBluetooth::sendReturnNotification(const String &studentId) {
  String msg = "RETURN," + studentId;
  return sendMessage(msg);
}

String LockerBluetooth::readResponse() {
  if (!isConnected()) {
    return "";
  }
  
  // Check if data is available (same pattern as working code)
  if (_bt.available()) {
    String msg = _bt.readStringUntil('\n');
    msg.trim();
    
    if (msg.length() > 0) {
      Serial.print("[BT] Received from Pi: '");
      Serial.print(msg);
      Serial.println("'");
      
      msg.toUpperCase();

      // Normalize responses
      if (msg == "OK" || msg == "GRANTED" || msg == "SUCCESS") {
        return "OK";
      }
      if (msg == "DENIED" || msg == "NO" || msg == "FAIL" || msg == "ERROR") {
        return "DENIED";
      }
      
      // Return as-is for other messages
      return msg;
    }
  }

  return "";  // No message available
}
