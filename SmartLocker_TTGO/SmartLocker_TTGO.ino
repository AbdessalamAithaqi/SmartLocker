/*
 * MINIMAL BLUETOOTH TEST FOR TTGO ESP32
 * 
 * This sketch does ONE thing: connect to your Raspberry Pi and send/receive messages
 * No sensors, no servos, no complexity - just pure Bluetooth testing
 * 
 * BEFORE UPLOADING:
 * 1. Get your Pi's MAC address (run: hciconfig on Pi)
 * 2. Replace PI_MAC_ADDRESS below with your actual MAC
 * 
 * EXPECTED BEHAVIOR:
 * - TTGO connects to Pi
 * - Sends "HELLO" every 5 seconds
 * - Prints Pi's responses to Serial Monitor
 */

#include <BluetoothSerial.h>

// ========================================
// CONFIGURATION - CHANGE THIS!
// ========================================
const char* PI_MAC_ADDRESS = "D8:3A:DD:D2:34:EC";
// REPLACE WITH YOUR PI'S MAC!
// ========================================
// GLOBALS
// ========================================
BluetoothSerial SerialBT;
bool connected = false;
unsigned long lastAttempt = 0;
unsigned long lastMessage = 0;

// ========================================
// SETUP
// ========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("TTGO ESP32 - MINIMAL BLUETOOTH TEST");
  Serial.println("========================================");
  Serial.println();
  Serial.print("Target Pi MAC: ");
  Serial.println(PI_MAC_ADDRESS);
  Serial.println();
  
  // Initialize Bluetooth as CLIENT (not master)
  SerialBT.begin("TTGO_Test", false);
  Serial.println("✓ Bluetooth initialized");
  Serial.println("Attempting to connect to Pi...");
  Serial.println();
}

// ========================================
// MAIN LOOP
// ========================================
void loop() {
  // STEP 1: Try to connect if not connected (every 5 seconds)
  if (!connected) {
    if (millis() - lastAttempt > 5000) {
      lastAttempt = millis();
      
      Serial.println("--------------------------------------");
      Serial.print("Connecting to ");
      Serial.print(PI_MAC_ADDRESS);
      Serial.println("...");
      
      connected = SerialBT.connect(PI_MAC_ADDRESS);
      
      if (connected) {
        Serial.println("✓✓✓ SUCCESS! Connected to Pi! ✓✓✓");
        Serial.println("--------------------------------------");
        Serial.println();
      } else {
        Serial.println("✗ Connection failed");
        Serial.println("Troubleshooting:");
        Serial.println("  1. Is Pi running pi_server.py?");
        Serial.println("  2. Is Pi Bluetooth on? (bluetoothctl power on)");
        Serial.println("  3. Is MAC address correct?");
        Serial.println("  4. Are devices within range?");
        Serial.println();
        Serial.println("Will retry in 5 seconds...");
        Serial.println();
      }
    }
    return;
  }
  
  // STEP 2: Check connection is still alive
  if (!SerialBT.connected()) {
    Serial.println("✗ Connection lost! Will retry...");
    connected = false;
    return;
  }
  
  // STEP 3: Send test message every 5 seconds
  if (millis() - lastMessage > 5000) {
    lastMessage = millis();
    
    Serial.print("→ Sending: HELLO");
    SerialBT.println("HELLO");
    Serial.println(" ...sent!");
  }
  
  // STEP 4: Read any responses from Pi
  while (SerialBT.available()) {
    char c = SerialBT.read();
    Serial.print(c);
  }
  
  delay(100);
}

// ========================================
// HELPER: Get your Pi's MAC via Serial
// ========================================
/*
 * On your Raspberry Pi, run one of these commands:
 * 
 * Method 1 (simple):
 *   hciconfig
 * 
 * Method 2 (detailed):
 *   bluetoothctl
 *   > show
 *   > exit
 * 
 * Method 3 (from script):
 *   sudo hcitool dev
 * 
 * Look for MAC address like: D8:3A:DD:D2:34:EC
 * Copy it exactly as shown (with colons)
 */