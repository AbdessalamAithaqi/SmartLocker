#include "config.h"
#include "lcd_display.h"
#include "keypad.h"
#include "led.h"
#include "servo.h"
#include "ir.h"
#include "bt_comm.h"

// ============================================
// STATES
// ============================================

// Main Locker States
enum class LockerState {
  IDLE_AVAILABLE,        // Locker has box, ready for borrow
  IDLE_OCCUPIED,         // Locker empty, box is borrowed
  AWAITING_ID,           // Waiting for student ID input
  AUTHENTICATING,        // Waiting for server response
  BORROW_AUTHORIZED,     // Open for borrowing
  BORROW_IN_PROGRESS,    // Door open, waiting for box removal
  BORROW_COMPLETING,     // Waiting for door close after borrow
  RETURN_IN_PROGRESS,    // Door open for return
  RETURN_COMPLETING,     // Waiting for box placement and door close
  ERROR_STATE,           // Error condition
  MAINTENANCE            // Maintenance mode
};

// Communication States
enum class CommState {
  IDLE,
  CONNECTING,
  SENDING_AUTH,
  WAITING_RESPONSE,
  PROCESSING_RESPONSE,
  OFFLINE_MODE,
  ERROR
};

// ============================================
// GLOBAL SENSORS AND ACTUATORS
// ============================================
LCDDisplay lcd;
LockerKeypad keypad;
GreenLED greenLed;
RedLED redLed;
DoorServo doorServo;
BoxIR boxSensor;
DoorIR doorSensor;
LockerBluetooth bluetooth(PI_BT_ADDRESS);  // Connect to Pi

// ============================================
// GLOBAL VARIABLES
// ============================================
LockerState currentLockerState = LockerState::IDLE_AVAILABLE;
CommState currentCommState = CommState::IDLE;

// Timing variables
unsigned long stateEntryTime = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long inputStartTime = 0;
unsigned long authStartTime = 0;
unsigned long lastConnectionAttempt = 0;
unsigned long lastStatusCheck = 0;

// Data variables
String currentStudentID = "";
String inputBuffer = "";
String lastBorrowerID = "";
bool isNetworkAvailable = false;  // Start as offline, will connect
bool authorizationPending = false;
bool authorizationResult = false;

// Sensor states
enum class SecurityState {
  DOOR_CLOSED_BOX_PRESENT,
  DOOR_CLOSED_BOX_ABSENT,
  DOOR_OPEN_BOX_PRESENT,
  DOOR_OPEN_BOX_ABSENT
};

SecurityState currentSecurityState = SecurityState::DOOR_CLOSED_BOX_PRESENT;
bool lastDoorState = false;
bool lastBoxState = false;
unsigned long doorDebounceTime = 0;
unsigned long boxDebounceTime = 0;

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n===========================================");
  Serial.println("Smart Locker System Starting...");
  Serial.println("===========================================");
  
  // Check if Pi address is set
  if (String(PI_BT_ADDRESS) == "AA:BB:CC:DD:EE:FF") {
    Serial.println("⚠️  WARNING: PI_BT_ADDRESS not set in config.h!");
    Serial.println("   Run 'hcitool dev' on your Pi to get the MAC address");
  }
  
  // Initialize hardware
  lcd.begin();
  keypad.begin();
  greenLed.begin();
  redLed.begin();
  doorServo.begin();
  boxSensor.begin();
  doorSensor.begin();
  bluetooth.begin();
  
  // Initial state setup
  doorServo.lock();
  greenLed.on();
  redLed.off();
  
  // Initial display
  lcd.printLines("Smart Locker", "Initializing...");
  delay(2000);
  
  // Check initial sensor states
  updateSecurityState();
  
  // Set initial locker state based on box presence
  if (boxSensor.isBoxPresent()) {
    currentLockerState = LockerState::IDLE_AVAILABLE;
    lcd.printLines("Ready", "Connecting to Pi");
  } else {
    currentLockerState = LockerState::IDLE_OCCUPIED;
    lcd.printLines("Box Borrowed", "Connecting to Pi");
  }
  
  // Try initial connection
  Serial.println("Attempting initial connection to Pi...");
  if (bluetooth.connect()) {
    isNetworkAvailable = true;
    Serial.println("✓ Initial connection successful");
  } else {
    Serial.println("✗ Initial connection failed - will retry");
  }
  
  lastConnectionAttempt = millis();
  Serial.println("System Ready\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Maintain connection
  maintainConnection();
  
  // Run state machines
  updateSecurityState();
  processLockerStateMachine();
  processCommStateMachine();
  
  // Update status display periodically
  if (millis() - lastDisplayUpdate > 10000) {
    updateStatusDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Check for emergency conditions
  handleEmergencyConditions();
}

// ============================================
// CONNECTION MANAGEMENT
// ============================================
void maintainConnection() {
  // Check connection status every 2 seconds
  if (millis() - lastStatusCheck > 2000) {
    lastStatusCheck = millis();
    
    bool wasAvailable = isNetworkAvailable;
    isNetworkAvailable = bluetooth.isConnected();
    
    // Log status changes
    if (wasAvailable != isNetworkAvailable) {
      if (isNetworkAvailable) {
        Serial.println("✓ Pi connection established");
      } else {
        Serial.println("✗ Pi connection lost");
      }
    }
  }
  
  // Try to reconnect if disconnected (but not too often)
  if (!isNetworkAvailable && 
      currentCommState == CommState::IDLE &&
      millis() - lastConnectionAttempt > BT_RECONNECT_INTERVAL_MS) {
    
    Serial.println("Attempting reconnection...");
    if (bluetooth.connect()) {
      isNetworkAvailable = true;
      Serial.println("✓ Reconnection successful");
    } else {
      Serial.println("✗ Reconnection failed");
    }
    lastConnectionAttempt = millis();
  }
}

// ============================================
// SECURITY STATE MACHINE
// ============================================
void updateSecurityState() {
  if (millis() - lastSensorCheck < SENSOR_DEBOUNCE_MS) {
    return;
  }
  lastSensorCheck = millis();
  
  bool doorClosed = doorSensor.isDoorClosed();
  bool boxPresent = boxSensor.isBoxPresent();
  
  // Debounce sensor readings
  if (doorClosed != lastDoorState) {
    if (millis() - doorDebounceTime > SENSOR_DEBOUNCE_MS) {
      lastDoorState = doorClosed;
      doorDebounceTime = millis();
    } else {
      return;
    }
  }
  
  if (boxPresent != lastBoxState) {
    if (millis() - boxDebounceTime > SENSOR_DEBOUNCE_MS) {
      lastBoxState = boxPresent;
      boxDebounceTime = millis();
    } else {
      return;
    }
  }
  
  // Update security state
  SecurityState newState;
  if (doorClosed && boxPresent) {
    newState = SecurityState::DOOR_CLOSED_BOX_PRESENT;
  } else if (doorClosed && !boxPresent) {
    newState = SecurityState::DOOR_CLOSED_BOX_ABSENT;
  } else if (!doorClosed && boxPresent) {
    newState = SecurityState::DOOR_OPEN_BOX_PRESENT;
  } else {
    newState = SecurityState::DOOR_OPEN_BOX_ABSENT;
  }
  
  if (newState != currentSecurityState) {
    currentSecurityState = newState;
  }
}

// ============================================
// MAIN LOCKER STATE MACHINE
// ============================================
void processLockerStateMachine() {
  switch (currentLockerState) {
    case LockerState::IDLE_AVAILABLE:
      handleIdleAvailable();
      break;
      
    case LockerState::IDLE_OCCUPIED:
      handleIdleOccupied();
      break;
      
    case LockerState::AWAITING_ID:
      handleAwaitingID();
      break;
      
    case LockerState::AUTHENTICATING:
      handleAuthenticating();
      break;
      
    case LockerState::BORROW_AUTHORIZED:
      handleBorrowAuthorized();
      break;
      
    case LockerState::BORROW_IN_PROGRESS:
      handleBorrowInProgress();
      break;
      
    case LockerState::BORROW_COMPLETING:
      handleBorrowCompleting();
      break;
      
    case LockerState::RETURN_IN_PROGRESS:
      handleReturnInProgress();
      break;
      
    case LockerState::RETURN_COMPLETING:
      handleReturnCompleting();
      break;
      
    case LockerState::ERROR_STATE:
      handleErrorState();
      break;
      
    case LockerState::MAINTENANCE:
      handleMaintenance();
      break;
  }
}

// ============================================
// LOCKER STATE HANDLERS
// ============================================
void handleIdleAvailable() {
  char key = keypad.getKey();
  if (key == '#') {
    currentLockerState = LockerState::AWAITING_ID;
    inputBuffer = "";
    inputStartTime = millis();
    lcd.printLines("Enter ID:", "");
    Serial.println("ID entry started");
  }
}

void handleIdleOccupied() {
  char key = keypad.getKey();
  if (key == '#') {
    // For returns, proceed even offline
    currentLockerState = LockerState::RETURN_IN_PROGRESS;
    doorServo.unlock();
    lcd.printLines("Door Unlocked", "Place box & close");
    greenLed.off();
    redLed.on();
    stateEntryTime = millis();
    Serial.println("Return initiated");
    
    // Send return message if connected
    if (isNetworkAvailable && lastBorrowerID != "") {
      bluetooth.sendCode("RETURN," + lastBorrowerID);
    }
  }
}

void handleAwaitingID() {
  // Check for timeout
  if (millis() - inputStartTime > INPUT_TIMEOUT_MS) {
    lcd.printLines("Timeout", "Try again");
    delay(2000);
    currentLockerState = LockerState::IDLE_AVAILABLE;
    updateStatusDisplay();
    Serial.println("ID entry timeout");
    return;
  }
  
  char key = keypad.getKey();
  if (key) {
    if (key >= '0' && key <= '9') {
      inputBuffer += key;
      lcd.printLines("Enter ID:", inputBuffer);
    } else if (key == '#') {
      if (inputBuffer.length() >= MIN_STUDENT_ID_LENGTH && 
          inputBuffer.length() <= MAX_STUDENT_ID_LENGTH) {
        currentStudentID = inputBuffer;
        Serial.print("ID entered: ");
        Serial.println(currentStudentID);
        currentLockerState = LockerState::AUTHENTICATING;
        requestAuthorization(currentStudentID);
      } else {
        lcd.printLines("Invalid ID", "4-12 digits");
        delay(2000);
        lcd.printLines("Enter ID:", "");
        inputBuffer = "";
      }
    } else if (key == '*') {
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        lcd.printLines("Enter ID:", inputBuffer);
      }
    }
  }
}

void handleAuthenticating() {
  // Wait for comm state machine to complete
  if (!authorizationPending) {
    if (authorizationResult) {
      Serial.println("✓ Authorization approved");
      currentLockerState = LockerState::BORROW_AUTHORIZED;
    } else {
      Serial.println("✗ Authorization denied");
      lcd.printLines("Access Denied", "Try again");
      delay(3000);
      currentLockerState = LockerState::IDLE_AVAILABLE;
      updateStatusDisplay();
    }
  }
  
  // Check for timeout
  if (millis() - authStartTime > AUTH_TIMEOUT_MS) {
    Serial.println("✗ Authorization timeout");
    authorizationPending = false;
    if (isNetworkAvailable) {
      lcd.printLines("Network Error", "Try again");
    } else {
      lcd.printLines("Offline Mode", "Borrow denied");
    }
    delay(3000);
    currentLockerState = LockerState::IDLE_AVAILABLE;
    updateStatusDisplay();
  }
}

void handleBorrowAuthorized() {
  doorServo.unlock();
  lcd.printLines("Authorized", "Take box & close");
  greenLed.off();
  redLed.on();
  stateEntryTime = millis();
  currentLockerState = LockerState::BORROW_IN_PROGRESS;
  Serial.println("Door unlocked for borrow");
}

void handleBorrowInProgress() {
  // Wait for box to be removed
  if (currentSecurityState == SecurityState::DOOR_OPEN_BOX_ABSENT) {
    lcd.printLines("Box removed", "Please close door");
    currentLockerState = LockerState::BORROW_COMPLETING;
    Serial.println("Box removed");
  }
  
  // Timeout check
  if (millis() - stateEntryTime > DOOR_OPEN_TIMEOUT_MS) {
    doorServo.lock();
    lcd.printLines("Timeout", "Transaction cancelled");
    greenLed.on();
    redLed.off();
    delay(2000);
    currentLockerState = LockerState::IDLE_AVAILABLE;
    updateStatusDisplay();
    Serial.println("Borrow timeout");
  }
}

void handleBorrowCompleting() {
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_ABSENT) {
    doorServo.lock();
    lastBorrowerID = currentStudentID;
    lcd.printLines("Borrow Complete", "ID: " + currentStudentID);
    greenLed.on();
    redLed.off();
    Serial.print("✓ Borrow complete: ");
    Serial.println(currentStudentID);
    
    delay(3000);
    currentLockerState = LockerState::IDLE_OCCUPIED;
    updateStatusDisplay();
  }
  
  // Check if box was put back (cancelled)
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_PRESENT) {
    doorServo.lock();
    lcd.printLines("Cancelled", "Box detected");
    greenLed.on();
    redLed.off();
    Serial.println("Borrow cancelled");
    delay(2000);
    currentLockerState = LockerState::IDLE_AVAILABLE;
    updateStatusDisplay();
  }
}

void handleReturnInProgress() {
  // Wait for box to be placed and door closed
  if (currentSecurityState == SecurityState::DOOR_OPEN_BOX_PRESENT) {
    lcd.printLines("Box detected", "Please close door");
    currentLockerState = LockerState::RETURN_COMPLETING;
    Serial.println("Box placed for return");
  }
  
  // Timeout check
  if (millis() - stateEntryTime > DOOR_OPEN_TIMEOUT_MS) {
    doorServo.lock();
    lcd.printLines("Timeout", "Try again");
    greenLed.on();
    redLed.off();
    delay(2000);
    currentLockerState = LockerState::IDLE_OCCUPIED;
    updateStatusDisplay();
    Serial.println("Return timeout");
  }
}

void handleReturnCompleting() {
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_PRESENT) {
    doorServo.lock();
    lcd.printLines("Return Complete", "Thank you!");
    greenLed.on();
    redLed.off();
    Serial.println("✓ Return complete");
    
    lastBorrowerID = "";
    delay(3000);
    currentLockerState = LockerState::IDLE_AVAILABLE;
    updateStatusDisplay();
  }
  
  // Check if box was removed again
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_ABSENT) {
    doorServo.lock();
    lcd.printLines("Return Failed", "Box not detected");
    greenLed.on();
    redLed.off();
    Serial.println("Return failed - no box");
    delay(2000);
    currentLockerState = LockerState::IDLE_OCCUPIED;
    updateStatusDisplay();
  }
}

void handleErrorState() {
  static unsigned long lastFlash = 0;
  if (millis() - lastFlash > 500) {
    redLed.on();
    delay(250);
    redLed.off();
    lastFlash = millis();
  }
  
  if (keypad.getKey() == 'D') {
    currentLockerState = LockerState::MAINTENANCE;
    Serial.println("Entering maintenance mode");
  }
}

void handleMaintenance() {
  lcd.printLines("Maintenance Mode", "D=exit A=unlock");
  
  char key = keypad.getKey();
  if (key == 'A') {
    doorServo.unlock();
    lcd.printLines("Door Unlocked", "Manual control");
    Serial.println("Manual unlock");
  } else if (key == 'B') {
    doorServo.lock();
    lcd.printLines("Door Locked", "Manual control");
    Serial.println("Manual lock");
  } else if (key == 'D') {
    // Exit maintenance
    updateSecurityState();
    if (boxSensor.isBoxPresent()) {
      currentLockerState = LockerState::IDLE_AVAILABLE;
    } else {
      currentLockerState = LockerState::IDLE_OCCUPIED;
    }
    updateStatusDisplay();
    Serial.println("Exiting maintenance mode");
  }
}

// ============================================
// COMMUNICATION STATE MACHINE
// ============================================
void processCommStateMachine() {
  switch (currentCommState) {
    case CommState::IDLE:
      // Nothing to do
      break;
      
    case CommState::CONNECTING:
      if (bluetooth.isConnected()) {
        Serial.println("✓ Connected, sending auth");
        currentCommState = CommState::SENDING_AUTH;
      } else if (millis() - authStartTime > BT_CONNECT_TIMEOUT_MS) {
        Serial.println("✗ Connection timeout");
        currentCommState = CommState::ERROR;
        isNetworkAvailable = false;
      }
      break;
      
    case CommState::SENDING_AUTH:
      if (bluetooth.sendCode(currentStudentID)) {
        Serial.println("Auth request sent");
        currentCommState = CommState::WAITING_RESPONSE;
        authStartTime = millis();
      } else {
        Serial.println("Failed to send auth");
        currentCommState = CommState::ERROR;
      }
      break;
      
    case CommState::WAITING_RESPONSE:
      {
        String response = bluetooth.readResponse();
        if (response != "") {
          Serial.print("Response: ");
          Serial.println(response);
          currentCommState = CommState::PROCESSING_RESPONSE;
          if (response == "OK") {
            authorizationResult = true;
          } else {
            authorizationResult = false;
          }
          authorizationPending = false;
          currentCommState = CommState::IDLE;
        } else if (millis() - authStartTime > AUTH_TIMEOUT_MS) {
          Serial.println("Response timeout");
          currentCommState = CommState::ERROR;
          authorizationPending = false;
          authorizationResult = false;
        }
      }
      break;
      
    case CommState::PROCESSING_RESPONSE:
      currentCommState = CommState::IDLE;
      break;
      
    case CommState::OFFLINE_MODE:
      authorizationPending = false;
      authorizationResult = false;
      currentCommState = CommState::IDLE;
      break;
      
    case CommState::ERROR:
      isNetworkAvailable = false;
      authorizationPending = false;
      authorizationResult = false;
      currentCommState = CommState::IDLE;
      break;
  }
}

// ============================================
// HELPER FUNCTIONS
// ============================================
void requestAuthorization(const String &studentID) {
  Serial.print("Requesting authorization for: ");
  Serial.println(studentID);
  
  lcd.printLines("Authorizing...", "Please wait");
  authorizationPending = true;
  authStartTime = millis();
  
  // Try to connect if not connected
  if (!bluetooth.isConnected()) {
    lcd.printLines("Connecting...", "Please wait");
    if (bluetooth.connect()) {
      isNetworkAvailable = true;
      currentCommState = CommState::SENDING_AUTH;
    } else {
      Serial.println("Connection failed");
      currentCommState = CommState::ERROR;
    }
  } else {
    currentCommState = CommState::SENDING_AUTH;
  }
}

void updateStatusDisplay() {
  String line1, line2;
  
  switch (currentLockerState) {
    case LockerState::IDLE_AVAILABLE:
      line1 = "Available";
      line2 = isNetworkAvailable ? "Press # to borrow" : "Press # (offline)";
      break;
      
    case LockerState::IDLE_OCCUPIED:
      line1 = "Occupied";
      line2 = "Press # to return";
      break;
      
    default:
      return;  // Keep current display
  }
  
  lcd.printLines(line1, line2);
}

void handleEmergencyConditions() {
  // Check for sensor failures
  static int sensorFailCount = 0;
  
  if (doorSensor.readRaw() < 100 || boxSensor.readRaw() < 100) {
    sensorFailCount++;
    if (sensorFailCount > 10) {
      lcd.printLines("Sensor Error", "Maintenance needed");
      currentLockerState = LockerState::ERROR_STATE;
      Serial.println("⚠️  SENSOR ERROR");
    }
  } else {
    sensorFailCount = 0;
  }
}