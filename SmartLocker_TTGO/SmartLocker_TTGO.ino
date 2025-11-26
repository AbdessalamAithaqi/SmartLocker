/*
 * TTGO ESP32 AS BLUETOOTH SERVER
 * 
 * The TTGO is now the SERVER - it waits for the Pi to connect to it.
 * This is simpler and often more reliable!
 * 
 * SETUP:
 * 1. Upload this to your TTGO
 * 2. Open Serial Monitor (115200 baud)
 * 3. Note the TTGO's MAC address shown
 * 4. Run the modified Pi client script with the TTGO's MAC
 */

#include <BluetoothSerial.h>

BluetoothSerial SerialBT;
String rxBuffer = "";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("TTGO ESP32 - BLUETOOTH SERVER");
  Serial.println("========================================");
  Serial.println();
  
  // Start Bluetooth as SERVER
  SerialBT.begin("SmartLocker_Server", true);  // true = master/server mode
  
  Serial.println("✓ Bluetooth server started");
  Serial.println();
  Serial.println("IMPORTANT: Your TTGO's MAC address:");
  
  // Print MAC address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  Serial.printf("   %02X:%02X:%02X:%02X:%02X:%02X\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  Serial.println();
  Serial.println("Copy this MAC address and use it in pi_client.py");
  Serial.println();
  Serial.println("========================================");
  Serial.println("Waiting for Pi to connect...");
  Serial.println("========================================");
  Serial.println();
}

void loop() {
  // Check if connected
  if (SerialBT.hasClient()) {
    // Read incoming data
    while (SerialBT.available()) {
      char c = SerialBT.read();
      
      if (c == '\n') {
        // Complete message received
        rxBuffer.trim();
        
        if (rxBuffer.length() > 0) {
          Serial.print("← Received: ");
          Serial.println(rxBuffer);
          
          // Echo back with "OK:"
          String response = "OK: " + rxBuffer;
          SerialBT.println(response);
          
          Serial.print("→ Sent: ");
          Serial.println(response);
        }
        
        rxBuffer = "";
      } else {
        rxBuffer += c;
      }
    }
  } else {
    // No client connected - show waiting message every 5 seconds
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 5000) {
      Serial.println("Still waiting for Pi connection...");
      lastMsg = millis();
    }
  }
  
  delay(10);
}