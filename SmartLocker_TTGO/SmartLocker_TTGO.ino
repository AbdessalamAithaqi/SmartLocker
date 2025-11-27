// SmartLocker_TTGO.ino
// ESP32 TTGO Smart Locker with Bluetooth SERVER mode
// Pi connects to us via rfcomm

#include "config.h"
#include "lcd_display.h"
#include "keypad.h"
#include "led.h"
#include "servo.h"
#include "ir.h"
#include "bt_comm.h"

// ============================================
// STATE DEFINITIONS
// ============================================
enum class State {
  INIT,
  IDLE_AVAILABLE,       // Box is present, ready for borrow
  IDLE_OCCUPIED,        // Box is out, waiting for return
  AWAITING_ID,          // User entering student ID
  AUTHENTICATING,       // Waiting for Pi response
  BORROW_AUTHORIZED,    // Borrow approved, unlocking door
  BORROW_IN_PROGRESS,   // Door open, waiting for box removal
  BORROW_COMPLETING,    // Box removed, waiting for door close
  RETURN_IN_PROGRESS,   // Door open, waiting for box placement
  RETURN_COMPLETING,    // Box placed, waiting for door close
  ERROR                 // Error state
};

// ============================================
// GLOBAL OBJECTS
// ============================================
LCDDisplay lcd;
LockerKeypad keypad;
GreenLED greenLed;
DoorServo doorServo;
BoxIR boxSensor;
DoorIR doorSensor;
LockerBluetooth bluetooth("SmartLockerTTGO");  // BT Server name

// ============================================
// STATE VARIABLES
// ============================================
State currentState = State::INIT;
State previousState = State::INIT;

String currentStudentID = "";
String inputBuffer = "";
String lastBorrowerID = "";

unsigned long stateEntryTime = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;

bool boxPresent = false;
bool doorClosed = true;
bool authRequestSent = false;

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=================================");
  Serial.println("Smart Locker TTGO Starting...");
  Serial.println("Mode: Bluetooth SERVER");
  Serial.println("=================================");
  
  // Initialize hardware
  lcd.begin();
  lcd.printLines("Smart Locker", "Initializing...");
  
  keypad.begin();
  greenLed.begin();
  doorServo.begin();
  boxSensor.begin();
  doorSensor.begin();
  
  // Initialize Bluetooth SERVER
  bluetooth.begin();
  
  // Initial state
  doorServo.lock();
  delay(1000);
  
  // Check initial sensor states
  updateSensors();
  
  // Determine starting state based on box presence
  if (boxPresent) {
    changeState(State::IDLE_AVAILABLE);
  } else {
    changeState(State::IDLE_OCCUPIED);
  }
  
  Serial.println("\nSystem Ready!");
  Serial.println("Waiting for Pi connection via Bluetooth...\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Update sensors periodically
  if (millis() - lastSensorRead > SENSOR_DEBOUNCE_MS) {
    updateSensors();
    lastSensorRead = millis();
  }
  
  // Process current state
  processState();
  
  // Check for state timeouts
  checkTimeouts();
}

// ============================================
// STATE MACHINE
// ============================================
void processState() {
  switch (currentState) {
    case State::INIT:
      changeState(State::IDLE_AVAILABLE);
      break;
      
    case State::IDLE_AVAILABLE:
      handleIdleAvailable();
      break;
      
    case State::IDLE_OCCUPIED:
      handleIdleOccupied();
      break;
      
    case State::AWAITING_ID:
      handleAwaitingID();
      break;
      
    case State::AUTHENTICATING:
      handleAuthenticating();
      break;
      
    case State::BORROW_AUTHORIZED:
      handleBorrowAuthorized();
      break;
      
    case State::BORROW_IN_PROGRESS:
      handleBorrowInProgress();
      break;
      
    case State::BORROW_COMPLETING:
      handleBorrowCompleting();
      break;
      
    case State::RETURN_IN_PROGRESS:
      handleReturnInProgress();
      break;
      
    case State::RETURN_COMPLETING:
      handleReturnCompleting();
      break;
      
    case State::ERROR:
      handleError();
      break;
  }
}

// ============================================
// STATE HANDLERS
// ============================================

void handleIdleAvailable() {
  // Update display periodically
  if (millis() - lastDisplayUpdate > 3000) {
    bool piConnected = bluetooth.isConnected();
    
    if (piConnected) {
      lcd.printLines("Box Available", "Press # to start");
      greenLed.on();
    } else {
      lcd.printLines("Waiting for Pi", "Connection...");
      // Blink green to indicate waiting
      static bool ledState = false;
      ledState = !ledState;
      if (ledState) greenLed.on(); else greenLed.off();
    }
    lastDisplayUpdate = millis();
  }
  
  // Only allow borrow if Pi is connected
  char key = keypad.getKey();
  if (key == '#') {
    if (bluetooth.isConnected()) {
      Serial.println("Starting borrow process");
      inputBuffer = "";
      changeState(State::AWAITING_ID);
    } else {
      lcd.printLines("Pi not connected", "Please wait...");
      delay(2000);
    }
  }
}

void handleIdleOccupied() {
  // Update display periodically
  if (millis() - lastDisplayUpdate > 3000) {
    lcd.printLines("Box Out", "# to return");
    greenLed.off();
    lastDisplayUpdate = millis();
  }
  
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println("Starting return process");
    changeState(State::RETURN_IN_PROGRESS);
  }
}

void handleAwaitingID() {
  // First entry - show prompt
  static bool promptShown = false;
  if (!promptShown) {
    lcd.printLines("Enter Student ID", "# to confirm");
    promptShown = true;
  }
  
  char key = keypad.getKey();
  
  if (key) {
    if (key >= '0' && key <= '9') {
      // Digit entered
      if (inputBuffer.length() < MAX_STUDENT_ID_LENGTH) {
        inputBuffer += key;
        lcd.printLines("Enter ID:", inputBuffer);
        Serial.print("Input: ");
        Serial.println(inputBuffer);
      }
      
    } else if (key == '#') {
      // Confirm entry
      if (inputBuffer.length() >= MIN_STUDENT_ID_LENGTH && 
          inputBuffer.length() <= MAX_STUDENT_ID_LENGTH) {
        currentStudentID = inputBuffer;
        Serial.print("ID entered: ");
        Serial.println(currentStudentID);
        promptShown = false;
        authRequestSent = false;
        changeState(State::AUTHENTICATING);
      } else {
        lcd.printLines("Invalid ID", "8-9 digits req'd");
        delay(2000);
        inputBuffer = "";
        lcd.printLines("Enter ID:", "");
      }
      
    } else if (key == '*') {
      // Backspace
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        lcd.printLines("Enter ID:", inputBuffer.length() > 0 ? inputBuffer : "");
      }
      
    } else if (key == 'D') {
      // Cancel
      promptShown = false;
      inputBuffer = "";
      changeState(State::IDLE_AVAILABLE);
    }
  }
}

void handleAuthenticating() {
  // Send request if not sent yet
  if (!authRequestSent) {
    lcd.printLines("Authorizing...", currentStudentID);
    greenLed.off();
    
    // Check Pi connection first
    if (!bluetooth.isConnected()) {
      Serial.println("Pi not connected!");
      lcd.printLines("Pi Offline", "Try again later");
      delay(3000);
      authRequestSent = false;
      changeState(State::IDLE_AVAILABLE);
      return;
    }
    
    // Send borrow request to Pi
    if (bluetooth.sendBorrowRequest(currentStudentID)) {
      Serial.println("Borrow request sent, waiting for response...");
      authRequestSent = true;
    } else {
      Serial.println("Failed to send request");
      lcd.printLines("Send Failed", "Try again");
      delay(2000);
      authRequestSent = false;
      changeState(State::IDLE_AVAILABLE);
      return;
    }
  }
  
  // Check for response from Pi
  String response = bluetooth.readResponse();
  
  if (response == "OK") {
    Serial.println("AUTHORIZED!");
    lcd.printLines("Authorized!", currentStudentID);
    greenLed.on();
    delay(1000);
    authRequestSent = false;
    changeState(State::BORROW_AUTHORIZED);
    
  } else if (response == "DENIED") {
    Serial.println("DENIED!");
    lcd.printLines("Access Denied", "Already borrowed");
    greenLed.off();
    delay(3000);
    authRequestSent = false;
    changeState(State::IDLE_AVAILABLE);
    
  } else if (response != "") {
    // Unexpected response
    Serial.print("Unexpected response: ");
    Serial.println(response);
  }
  
  // Blink green LED while waiting
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 300) {
    static bool blinkState = false;
    blinkState = !blinkState;
    if (blinkState) {
      greenLed.on();
    } else {
      greenLed.off();
    }
    lastBlink = millis();
  }
}

void handleBorrowAuthorized() {
  // Unlock door for user to take box
  doorServo.unlock();
  lcd.printLines("Door Unlocked", "Take box & close");
  greenLed.on();
  
  changeState(State::BORROW_IN_PROGRESS);
}

void handleBorrowInProgress() {
  // Wait for user to open door and take box
  
  if (!doorClosed) {
    // Door is open
    if (!boxPresent) {
      // Box has been removed
      Serial.println("Box removed!");
      lcd.printLines("Box Taken!", "Please close door");
      changeState(State::BORROW_COMPLETING);
      return;
    } else {
      lcd.printLines("Door Open", "Take the box");
    }
  } else {
    // Door still closed
    lcd.printLines("Open Door", "Take box inside");
  }
  
  // Blink green LED
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    static bool blinkState = false;
    blinkState = !blinkState;
    if (blinkState) greenLed.on(); else greenLed.off();
    lastBlink = millis();
  }
}

void handleBorrowCompleting() {
  // Box removed, waiting for door to close
  
  if (doorClosed && !boxPresent) {
    // Door closed, box gone - success!
    doorServo.lock();
    lastBorrowerID = currentStudentID;
    
    Serial.print("Borrow complete! Student: ");
    Serial.println(currentStudentID);
    
    lcd.printLines("Success!", "Enjoy your kit");
    greenLed.on();
    delay(3000);
    
    currentStudentID = "";
    changeState(State::IDLE_OCCUPIED);
    
  } else if (!doorClosed) {
    lcd.printLines("Close Door", "To complete");
  } else if (boxPresent) {
    // Box was put back? Go back to in-progress
    lcd.printLines("Take the box", "Then close door");
    changeState(State::BORROW_IN_PROGRESS);
  }
}

void handleReturnInProgress() {
  static bool doorUnlocked = false;
  
  if (!doorUnlocked) {
    doorServo.unlock();
    lcd.printLines("Door Unlocked", "Place box inside");
    greenLed.off();
    doorUnlocked = true;
  }
  
  if (!doorClosed) {
    // Door is open
    if (boxPresent) {
      // Box detected!
      Serial.println("Box placed in locker!");
      lcd.printLines("Box Detected!", "Close door");
      doorUnlocked = false;
      changeState(State::RETURN_COMPLETING);
      return;
    } else {
      lcd.printLines("Door Open", "Place box inside");
    }
  } else {
    lcd.printLines("Open Door", "Place box inside");
  }
  
  // Blink green LED
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    static bool blinkState = false;
    blinkState = !blinkState;
    if (blinkState) greenLed.on(); else greenLed.off();
    lastBlink = millis();
  }
}

void handleReturnCompleting() {
  if (doorClosed && boxPresent) {
    // Door closed, box present - success!
    doorServo.lock();
    
    // Notify Pi about return
    if (bluetooth.isConnected() && lastBorrowerID.length() > 0) {
      bluetooth.sendReturnNotification(lastBorrowerID);
      Serial.print("Return logged for: ");
      Serial.println(lastBorrowerID);
    }
    
    lcd.printLines("Return Complete", "Thank you!");
    greenLed.on();
    delay(3000);
    
    lastBorrowerID = "";
    changeState(State::IDLE_AVAILABLE);
    
  } else if (!doorClosed) {
    lcd.printLines("Close Door", "To complete");
  } else if (!boxPresent) {
    // Box was removed again?
    lcd.printLines("Place box", "Then close door");
    changeState(State::RETURN_IN_PROGRESS);
  }
}

void handleError() {
  // Blink green LED rapidly to indicate error
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 200) {
    static bool blinkState = false;
    blinkState = !blinkState;
    if (blinkState) greenLed.on(); else greenLed.off();
    lastBlink = millis();
  }
  
  lcd.printLines("ERROR!", "D to restart");
  
  char key = keypad.getKey();
  if (key == 'D') {
    Serial.println("Restarting...");
    ESP.restart();
  }
}

// ============================================
// HELPER FUNCTIONS
// ============================================

void changeState(State newState) {
  if (currentState != newState) {
    Serial.print("[STATE] ");
    Serial.print(stateToString(currentState));
    Serial.print(" -> ");
    Serial.println(stateToString(newState));
    
    previousState = currentState;
    currentState = newState;
    stateEntryTime = millis();
    lastDisplayUpdate = 0;  // Force display update
  }
}

const char* stateToString(State state) {
  switch(state) {
    case State::INIT:               return "INIT";
    case State::IDLE_AVAILABLE:     return "IDLE_AVAILABLE";
    case State::IDLE_OCCUPIED:      return "IDLE_OCCUPIED";
    case State::AWAITING_ID:        return "AWAITING_ID";
    case State::AUTHENTICATING:     return "AUTHENTICATING";
    case State::BORROW_AUTHORIZED:  return "BORROW_AUTHORIZED";
    case State::BORROW_IN_PROGRESS: return "BORROW_IN_PROGRESS";
    case State::BORROW_COMPLETING:  return "BORROW_COMPLETING";
    case State::RETURN_IN_PROGRESS: return "RETURN_IN_PROGRESS";
    case State::RETURN_COMPLETING:  return "RETURN_COMPLETING";
    case State::ERROR:              return "ERROR";
    default:                        return "UNKNOWN";
  }
}

void updateSensors() {
  // Read sensors with averaging for stability
  static int boxReadings[3] = {0, 0, 0};
  static int doorReadings[3] = {0, 0, 0};
  static int readIndex = 0;
  
  boxReadings[readIndex] = boxSensor.readRaw();
  doorReadings[readIndex] = doorSensor.readRaw();
  
  readIndex = (readIndex + 1) % 3;
  
  // Calculate averages
  int boxAvg = (boxReadings[0] + boxReadings[1] + boxReadings[2]) / 3;
  int doorAvg = (doorReadings[0] + doorReadings[1] + doorReadings[2]) / 3;
  
  bool newBoxPresent = (boxAvg >= IR_BOX_THRESHOLD);
  bool newDoorClosed = (doorAvg >= IR_DOOR_THRESHOLD);
  
  // Log changes
  if (newBoxPresent != boxPresent) {
    Serial.print("[SENSOR] Box: ");
    Serial.print(newBoxPresent ? "PRESENT" : "ABSENT");
    Serial.print(" (raw: ");
    Serial.print(boxAvg);
    Serial.println(")");
    boxPresent = newBoxPresent;
  }
  
  if (newDoorClosed != doorClosed) {
    Serial.print("[SENSOR] Door: ");
    Serial.print(newDoorClosed ? "CLOSED" : "OPEN");
    Serial.print(" (raw: ");
    Serial.print(doorAvg);
    Serial.println(")");
    doorClosed = newDoorClosed;
  }
}

void checkTimeouts() {
  unsigned long elapsed = millis() - stateEntryTime;
  
  switch (currentState) {
    case State::AWAITING_ID:
      if (elapsed > INPUT_TIMEOUT_MS) {
        Serial.println("[TIMEOUT] ID entry timeout");
        lcd.printLines("Timeout", "Try again");
        delay(2000);
        inputBuffer = "";
        changeState(State::IDLE_AVAILABLE);
      }
      break;
      
    case State::AUTHENTICATING:
      if (elapsed > AUTH_TIMEOUT_MS) {
        Serial.println("[TIMEOUT] Authentication timeout");
        lcd.printLines("No Response", "Try again later");
        delay(2000);
        authRequestSent = false;
        changeState(State::IDLE_AVAILABLE);
      }
      break;
      
    case State::BORROW_IN_PROGRESS:
    case State::BORROW_COMPLETING:
      if (elapsed > DOOR_OPEN_TIMEOUT_MS) {
        Serial.println("[TIMEOUT] Borrow timeout - locking door");
        doorServo.lock();
        lcd.printLines("Timeout!", "Door locked");
        delay(3000);
        
        // Check final state
        if (boxPresent) {
          changeState(State::IDLE_AVAILABLE);
        } else {
          // Box was taken, log the borrow
          lastBorrowerID = currentStudentID;
          changeState(State::IDLE_OCCUPIED);
        }
      }
      break;
      
    case State::RETURN_IN_PROGRESS:
    case State::RETURN_COMPLETING:
      if (elapsed > DOOR_OPEN_TIMEOUT_MS) {
        Serial.println("[TIMEOUT] Return timeout - locking door");
        doorServo.lock();
        lcd.printLines("Timeout!", "Door locked");
        delay(3000);
        
        // Check final state
        if (boxPresent) {
          // Return was successful
          if (bluetooth.isConnected() && lastBorrowerID.length() > 0) {
            bluetooth.sendReturnNotification(lastBorrowerID);
          }
          lastBorrowerID = "";
          changeState(State::IDLE_AVAILABLE);
        } else {
          changeState(State::IDLE_OCCUPIED);
        }
      }
      break;
      
    default:
      break;
  }
}
