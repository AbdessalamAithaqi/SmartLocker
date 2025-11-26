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

// User Interface States
enum class UIState {
  DISPLAY_STATUS,
  INPUT_ACTIVE,
  SHOWING_MESSAGE,
  ERROR_DISPLAY
};

// Physical Security States
enum class SecurityState {
  DOOR_CLOSED_BOX_PRESENT,
  DOOR_CLOSED_BOX_ABSENT,
  DOOR_OPEN_BOX_PRESENT,
  DOOR_OPEN_BOX_ABSENT,
  SENSOR_ERROR
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
LockerBluetooth bluetooth("SmartLocker");

// ============================================
// GLOBAL VARIABLES
// ============================================
LockerState currentLockerState = LockerState::IDLE_AVAILABLE;
CommState currentCommState = CommState::IDLE;
UIState currentUIState = UIState::DISPLAY_STATUS;
SecurityState currentSecurityState = SecurityState::DOOR_CLOSED_BOX_PRESENT;

// Timing variables
unsigned long stateEntryTime = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long inputStartTime = 0;
unsigned long authStartTime = 0;
unsigned long messageDisplayTime = 0;

// Data variables
String currentStudentID = "";
String inputBuffer = "";
String displayMessage = "";
String lastBorrowerID = "";
bool isNetworkAvailable = true;
bool authorizationPending = false;
bool authorizationResult = false;

// Sensor debouncing
bool lastDoorState = false;
bool lastBoxState = false;
unsigned long doorDebounceTime = 0;
unsigned long boxDebounceTime = 0;

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  Serial.println("Smart Locker System Starting...");
  
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
    lcd.printLines("Ready", "Press # to start");
  } else {
    currentLockerState = LockerState::IDLE_OCCUPIED;
    lcd.printLines("Box Borrowed", "ID: " + lastBorrowerID);
  }
  
  Serial.println("System Ready");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Run concurrent state machines
  updateSecurityState();      // Always monitor physical security
  processLockerStateMachine();
  processCommStateMachine();
  processUIStateMachine();
  
  // Check for system-wide events
  handleEmergencyConditions();
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
      return; // Still debouncing
    }
  }
  
  if (boxPresent != lastBoxState) {
    if (millis() - boxDebounceTime > SENSOR_DEBOUNCE_MS) {
      lastBoxState = boxPresent;
      boxDebounceTime = millis();
    } else {
      return; // Still debouncing
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
    Serial.print("Security State Change: ");
    Serial.println(static_cast<int>(newState));
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
  // Box is present, ready for borrowing
  char key = keypad.getKey();
  if (key == '#') {
    transitionToState(LockerState::AWAITING_ID);
    inputBuffer = "";
    inputStartTime = millis();
    lcd.printLines("Enter ID:", "");
  }
}

void handleIdleOccupied() {
  // Box is borrowed, ready for return
  char key = keypad.getKey();
  if (key == '#') {
    // For returns, we can proceed even offline
    transitionToState(LockerState::RETURN_IN_PROGRESS);
    doorServo.unlock();
    lcd.printLines("Door Unlocked", "Place box & close");
    greenLed.off();
    redLed.on();
    stateEntryTime = millis();
  }
}

void handleAwaitingID() {
  // Check for timeout
  if (millis() - inputStartTime > INPUT_TIMEOUT_MS) {
    lcd.printLines("Timeout", "Try again");
    transitionToState(LockerState::IDLE_AVAILABLE);
    return;
  }
  
  char key = keypad.getKey();
  if (key) {
    if (key >= '0' && key <= '9') {
      inputBuffer += key;
      lcd.printLines("Enter ID:", inputBuffer);
    } else if (key == '#') {
      if (inputBuffer.length() >= 8) { // Minimum ID length
        currentStudentID = inputBuffer;
        transitionToState(LockerState::AUTHENTICATING);
        requestAuthorization(currentStudentID);
      } else {
        lcd.printLines("Invalid ID", "Min 8 digits");
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
  // This state waits for the comm state machine to get a response
  if (authorizationPending == false) {
    if (authorizationResult) {
      transitionToState(LockerState::BORROW_AUTHORIZED);
    } else {
      lcd.printLines("Access Denied", "Try again");
      delay(2000);
      transitionToState(LockerState::IDLE_AVAILABLE);
    }
  }
  
  // Check for timeout
  if (millis() - authStartTime > AUTH_TIMEOUT_MS) {
    if (isNetworkAvailable) {
      lcd.printLines("Network Error", "Try again");
    } else {
      lcd.printLines("Offline Mode", "Borrow denied");
    }
    delay(2000);
    transitionToState(LockerState::IDLE_AVAILABLE);
  }
}

void handleBorrowAuthorized() {
  doorServo.unlock();
  lcd.printLines("Authorized", "Take box & close");
  greenLed.off();
  redLed.on();
  stateEntryTime = millis();
  transitionToState(LockerState::BORROW_IN_PROGRESS);
}

void handleBorrowInProgress() {
  // Wait for door to open and box to be removed
  if (currentSecurityState == SecurityState::DOOR_OPEN_BOX_ABSENT) {
    lcd.printLines("Box removed", "Please close door");
    transitionToState(LockerState::BORROW_COMPLETING);
  }
  
  // Timeout check
  if (millis() - stateEntryTime > DOOR_OPEN_TIMEOUT_MS) {
    doorServo.lock();
    lcd.printLines("Timeout", "Transaction cancelled");
    delay(2000);
    transitionToState(LockerState::IDLE_AVAILABLE);
  }
}

void handleBorrowCompleting() {
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_ABSENT) {
    doorServo.lock();
    lastBorrowerID = currentStudentID;
    lcd.printLines("Borrow Complete", "ID: " + currentStudentID);
    greenLed.on();
    redLed.off();
    
    // Log the transaction
    logTransaction("BORROW", currentStudentID);
    
    delay(3000);
    transitionToState(LockerState::IDLE_OCCUPIED);
  }
  
  // Check if box was put back (cancelled transaction)
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_PRESENT) {
    doorServo.lock();
    lcd.printLines("Cancelled", "Box detected");
    greenLed.on();
    redLed.off();
    delay(2000);
    transitionToState(LockerState::IDLE_AVAILABLE);
  }
}

void handleReturnInProgress() {
  // Wait for box to be placed and door closed
  if (currentSecurityState == SecurityState::DOOR_OPEN_BOX_PRESENT) {
    lcd.printLines("Box detected", "Please close door");
    transitionToState(LockerState::RETURN_COMPLETING);
  }
  
  // Timeout check
  if (millis() - stateEntryTime > DOOR_OPEN_TIMEOUT_MS) {
    doorServo.lock();
    lcd.printLines("Timeout", "Try again");
    delay(2000);
    transitionToState(LockerState::IDLE_OCCUPIED);
  }
}

void handleReturnCompleting() {
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_PRESENT) {
    doorServo.lock();
    lcd.printLines("Return Complete", "Thank you!");
    greenLed.on();
    redLed.off();
    
    // Log the transaction
    logTransaction("RETURN", lastBorrowerID);
    
    lastBorrowerID = "";
    delay(3000);
    transitionToState(LockerState::IDLE_AVAILABLE);
  }
  
  // Check if box was removed again
  if (currentSecurityState == SecurityState::DOOR_CLOSED_BOX_ABSENT) {
    doorServo.lock();
    lcd.printLines("Return Failed", "Box not detected");
    delay(2000);
    transitionToState(LockerState::IDLE_OCCUPIED);
  }
}

void handleErrorState() {
  // Flash red LED
  static unsigned long lastFlash = 0;
  if (millis() - lastFlash > 500) {
    redLed.on();
    delay(250);
    redLed.off();
    lastFlash = millis();
  }
  
  // Check for recovery conditions
  if (keypad.getKey() == 'D') { // Admin override
    transitionToState(LockerState::MAINTENANCE);
  }
}

void handleMaintenance() {
  lcd.printLines("Maintenance Mode", "Press D to exit");
  
  // Allow manual control in maintenance mode
  char key = keypad.getKey();
  if (key == 'A') {
    doorServo.unlock();
    lcd.printLines("Door Unlocked", "Manual control");
  } else if (key == 'B') {
    doorServo.lock();
    lcd.printLines("Door Locked", "Manual control");
  } else if (key == 'D') {
    // Exit maintenance mode
    updateSecurityState();
    if (boxSensor.isBoxPresent()) {
      transitionToState(LockerState::IDLE_AVAILABLE);
    } else {
      transitionToState(LockerState::IDLE_OCCUPIED);
    }
  }
}

// ============================================
// COMMUNICATION STATE MACHINE
// ============================================
void processCommStateMachine() {
  switch (currentCommState) {
    case CommState::IDLE:
      // Nothing to do, waiting for requests
      break;
      
    case CommState::CONNECTING:
      if (bluetooth.isConnected()) {
        currentCommState = CommState::SENDING_AUTH;
      } else if (millis() - authStartTime > 2000) {
        currentCommState = CommState::OFFLINE_MODE;
        isNetworkAvailable = false;
      }
      break;
      
    case CommState::SENDING_AUTH:
      if (bluetooth.sendCode(currentStudentID)) {
        currentCommState = CommState::WAITING_RESPONSE;
        authStartTime = millis();
      } else {
        currentCommState = CommState::ERROR;
      }
      break;
      
    case CommState::WAITING_RESPONSE:
      {
        String response = bluetooth.readResponse();
        if (response != "") {
          currentCommState = CommState::PROCESSING_RESPONSE;
          if (response == "OK") {
            authorizationResult = true;
          } else {
            authorizationResult = false;
          }
          authorizationPending = false;
          currentCommState = CommState::IDLE;
        } else if (millis() - authStartTime > AUTH_TIMEOUT_MS) {
          currentCommState = CommState::ERROR;
          authorizationPending = false;
          authorizationResult = false;
        }
      }
      break;
      
    case CommState::PROCESSING_RESPONSE:
      // Response already processed
      currentCommState = CommState::IDLE;
      break;
      
    case CommState::OFFLINE_MODE:
      // In offline mode, deny new borrows but allow returns
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
// UI STATE MACHINE
// ============================================
void processUIStateMachine() {
  switch (currentUIState) {
    case UIState::DISPLAY_STATUS:
      // Update display based on main state
      if (millis() - lastDisplayUpdate > 5000) {
        updateStatusDisplay();
        lastDisplayUpdate = millis();
      }
      break;
      
    case UIState::INPUT_ACTIVE:
      // Handled by main state machine
      break;
      
    case UIState::SHOWING_MESSAGE:
      if (millis() - messageDisplayTime > DISPLAY_MESSAGE_MS) {
        currentUIState = UIState::DISPLAY_STATUS;
      }
      break;
      
    case UIState::ERROR_DISPLAY:
      // Flash error message
      break;
  }
}

// ============================================
// HELPER FUNCTIONS
// ============================================
void transitionToState(LockerState newState) {
  Serial.print("State transition: ");
  Serial.print(static_cast<int>(currentLockerState));
  Serial.print(" -> ");
  Serial.println(static_cast<int>(newState));
  
  currentLockerState = newState;
  stateEntryTime = millis();
}

void requestAuthorization(const String &studentID) {
  Serial.println("Requesting authorization for ID: " + studentID);
  lcd.printLines("Authorizing...", "Please wait");
  authorizationPending = true;
  authStartTime = millis();
  currentCommState = CommState::CONNECTING;
}

void logTransaction(const String &type, const String &studentID) {
  Serial.print("LOG: ");
  Serial.print(type);
  Serial.print(" - ID: ");
  Serial.print(studentID);
  Serial.print(" - Time: ");
  Serial.println(millis());
  
  // Send log via Bluetooth if connected
  if (bluetooth.isConnected()) {
    String logEntry = type + "," + studentID + "," + String(millis());
    bluetooth.sendCode(logEntry);
  }
}

void updateStatusDisplay() {
  switch (currentLockerState) {
    case LockerState::IDLE_AVAILABLE:
      lcd.printLines("Available", "Press # to borrow");
      break;
      
    case LockerState::IDLE_OCCUPIED:
      if (lastBorrowerID != "") {
        lcd.printLines("Occupied", "Return: Press #");
      } else {
        lcd.printLines("Occupied", "Press # to return");
      }
      break;
      
    default:
      // Keep current display
      break;
  }
}

void handleEmergencyConditions() {
  // Check for sensor failures
  static int sensorFailCount = 0;
  
  // If sensors give impossible readings repeatedly
  if (doorSensor.readRaw() < 100 || boxSensor.readRaw() < 100) {
    sensorFailCount++;
    if (sensorFailCount > 10) {
      lcd.printLines("Sensor Error", "Maintenance needed");
      transitionToState(LockerState::ERROR_STATE);
    }
  } else {
    sensorFailCount = 0;
  }
  
  // Check for forced entry (door open when it should be locked)
  if (currentLockerState == LockerState::IDLE_AVAILABLE || 
      currentLockerState == LockerState::IDLE_OCCUPIED) {
    if (currentSecurityState == SecurityState::DOOR_OPEN_BOX_PRESENT ||
        currentSecurityState == SecurityState::DOOR_OPEN_BOX_ABSENT) {
      // Unauthorized door opening
      lcd.printLines("SECURITY ALERT", "Forced entry!");
      redLed.on();
      
      // Log security event
      logTransaction("SECURITY_BREACH", "UNKNOWN");
      
      // Try to recover
      if (doorSensor.isDoorClosed()) {
        doorServo.lock();
        updateSecurityState();
        if (boxSensor.isBoxPresent()) {
          transitionToState(LockerState::IDLE_AVAILABLE);
        } else {
          transitionToState(LockerState::IDLE_OCCUPIED);
        }
      }
    }
  }
}
