#include "config.h"
#include "lcd_display.h"
#include "keypad.h"
#include "led.h"
#include "servo.h"
#include "ir.h"
#include "bt_comm.h"

// STATE DEFINITIONS
enum class State {
  INIT,
  IDLE_AVAILABLE,       // Box is present, green LED, ready for borrow
  IDLE_OCCUPIED,        // Box is out, LED off, ready for return
  AWAITING_ID,          // User entering student ID (for borrow)
  AUTHENTICATING,       // Waiting for Pi response (for borrow)
  BORROW_AUTHORIZED,    // Borrow approved, unlocking door
  BORROW_IN_PROGRESS,   // Door open, waiting for box removal
  BORROW_COMPLETING,    // Box removed, waiting for door close
  AWAITING_RETURN_ID,   // User entering student ID (for return)
  RETURN_AUTHORIZED,    // Return approved, unlocking door  
  RETURN_IN_PROGRESS,   // Door open, waiting for box placement
  RETURN_COMPLETING,    // Box placed, waiting for door close
  ERROR                 // Error state
};

// GLOBAL OBJECTS
LCDDisplay lcd;
LockerKeypad keypad;
GreenLED greenLed;
DoorServo doorServo;
BoxIR boxSensor;
DoorIR doorSensor;
LockerBluetooth bluetooth("SmartLockerTTGO");

// STATE VARIABLES
State currentState = State::INIT;
State previousState = State::INIT;

String currentStudentID = "";
String inputBuffer = "";

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
  
  // Initialize Bluetooth
  bluetooth.begin();
  
  // Lock door initially
  doorServo.lock();
  
  // CRITICAL: Take multiple sensor readings to stabilize before deciding state
  Serial.println("Stabilizing sensors...");
  for (int i = 0; i < 10; i++) {
    boxPresent = boxSensor.isBoxPresent();
    doorClosed = doorSensor.isDoorClosed();
    delay(100);
  }
  
  Serial.print("Initial state - Box: ");
  Serial.print(boxPresent ? "PRESENT" : "ABSENT");
  Serial.print(", Door: ");
  Serial.println(doorClosed ? "CLOSED" : "OPEN");
  
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
      if (boxPresent) {
        changeState(State::IDLE_AVAILABLE);
      } else {
        changeState(State::IDLE_OCCUPIED);
      }
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
      
    case State::AWAITING_RETURN_ID:
      handleAwaitingReturnID();
      break;
      
    case State::RETURN_AUTHORIZED:
      handleReturnAuthorized();
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
// IDLE AVAILABLE - Box inside, ready for borrow
// ============================================
void handleIdleAvailable() {
  // Auto-correct if box was removed externally
  if (!boxPresent && doorClosed) {
    Serial.println("[AUTO] Box removed - switching to IDLE_OCCUPIED");
    changeState(State::IDLE_OCCUPIED);
    return;
  }
  
  // Update display periodically
  if (millis() - lastDisplayUpdate > 2000) {
    bool piConnected = bluetooth.isConnected();
    
    if (piConnected) {
      lcd.printLines("Box Available", "# to borrow");
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
  
  // Check for keypress
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

// ============================================
// IDLE OCCUPIED - Box is out, ready for return
// ============================================
void handleIdleOccupied() {
  // Auto-correct if box was placed externally
  if (boxPresent && doorClosed) {
    Serial.println("[AUTO] Box detected - switching to IDLE_AVAILABLE");
    changeState(State::IDLE_AVAILABLE);
    return;
  }
  
  // Update display periodically
  if (millis() - lastDisplayUpdate > 2000) {
    bool piConnected = bluetooth.isConnected();
    
    if (piConnected) {
      lcd.printLines("Locker Empty", "# to return");
    } else {
      lcd.printLines("Locker Empty", "Waiting for Pi");
    }
    greenLed.off();  // No box = LED off
    lastDisplayUpdate = millis();
  }
  
  // Check for keypress
  char key = keypad.getKey();
  if (key == '#') {
    if (bluetooth.isConnected()) {
      Serial.println("Starting return process");
      inputBuffer = "";
      changeState(State::AWAITING_RETURN_ID);
    } else {
      lcd.printLines("Pi not connected", "Please wait...");
      delay(2000);
    }
  }
}

// ============================================
// AWAITING ID - For BORROW
// ============================================
void handleAwaitingID() {
  static bool promptShown = false;
  if (!promptShown) {
    lcd.printLines("Enter Student ID", "# confirm * back");
    greenLed.on();
    promptShown = true;
  }
  
  char key = keypad.getKey();
  
  if (key) {
    if (key >= '0' && key <= '9') {
      if (inputBuffer.length() < MAX_STUDENT_ID_LENGTH) {
        inputBuffer += key;
        lcd.printLines("Borrow - ID:", inputBuffer);
        Serial.print("Input: ");
        Serial.println(inputBuffer);
      }
      
    } else if (key == '#') {
      if (inputBuffer.length() >= MIN_STUDENT_ID_LENGTH && 
          inputBuffer.length() <= MAX_STUDENT_ID_LENGTH) {
        currentStudentID = inputBuffer;
        Serial.print("ID entered for BORROW: ");
        Serial.println(currentStudentID);
        promptShown = false;
        authRequestSent = false;
        changeState(State::AUTHENTICATING);
      } else {
        lcd.printLines("Invalid ID", "8-9 digits req'd");
        delay(2000);
        inputBuffer = "";
        lcd.printLines("Borrow - ID:", "");
      }
      
    } else if (key == '*') {
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        lcd.printLines("Borrow - ID:", inputBuffer.length() > 0 ? inputBuffer : "");
      }
      
    } else if (key == 'D') {
      promptShown = false;
      inputBuffer = "";
      changeState(State::IDLE_AVAILABLE);
    }
  }
}

// ============================================
// AWAITING RETURN ID - For RETURN
// ============================================
void handleAwaitingReturnID() {
  static bool promptShown = false;
  if (!promptShown) {
    lcd.printLines("Return - Enter ID", "# confirm * back");
    greenLed.off();
    promptShown = true;
  }
  
  char key = keypad.getKey();
  
  if (key) {
    if (key >= '0' && key <= '9') {
      if (inputBuffer.length() < MAX_STUDENT_ID_LENGTH) {
        inputBuffer += key;
        lcd.printLines("Return - ID:", inputBuffer);
        Serial.print("Input: ");
        Serial.println(inputBuffer);
      }
      
    } else if (key == '#') {
      if (inputBuffer.length() >= MIN_STUDENT_ID_LENGTH && 
          inputBuffer.length() <= MAX_STUDENT_ID_LENGTH) {
        currentStudentID = inputBuffer;
        Serial.print("ID entered for RETURN: ");
        Serial.println(currentStudentID);
        promptShown = false;
        // For returns, we just unlock - the Pi will verify when we send RETURN
        changeState(State::RETURN_AUTHORIZED);
      } else {
        lcd.printLines("Invalid ID", "8-9 digits req'd");
        delay(2000);
        inputBuffer = "";
        lcd.printLines("Return - ID:", "");
      }
      
    } else if (key == '*') {
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        lcd.printLines("Return - ID:", inputBuffer.length() > 0 ? inputBuffer : "");
      }
      
    } else if (key == 'D') {
      promptShown = false;
      inputBuffer = "";
      changeState(State::IDLE_OCCUPIED);
    }
  }
}

// ============================================
// AUTHENTICATING - Check with Pi for BORROW
// ============================================
void handleAuthenticating() {
  if (!authRequestSent) {
    lcd.printLines("Checking...", currentStudentID);
    greenLed.off();
    
    if (!bluetooth.isConnected()) {
      Serial.println("Pi not connected!");
      lcd.printLines("Pi Offline", "Try again later");
      delay(3000);
      authRequestSent = false;
      changeState(State::IDLE_AVAILABLE);
      return;
    }
    
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
  
  String response = bluetooth.readResponse();
  
  if (response == "OK") {
    Serial.println("BORROW AUTHORIZED!");
    lcd.printLines("Approved!", "Opening door...");
    greenLed.on();
    delay(1000);
    authRequestSent = false;
    changeState(State::BORROW_AUTHORIZED);
    
  } else if (response == "DENIED") {
    Serial.println("BORROW DENIED!");
    lcd.printLines("Access Denied", "Not authorized");
    greenLed.off();
    delay(3000);
    authRequestSent = false;
    changeState(State::IDLE_AVAILABLE);
    
  } else if (response != "") {
    Serial.print("Unexpected response: ");
    Serial.println(response);
  }
  
  // Blink while waiting
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 300) {
    static bool blinkState = false;
    blinkState = !blinkState;
    if (blinkState) greenLed.on(); else greenLed.off();
    lastBlink = millis();
  }
}

// ============================================
// BORROW AUTHORIZED - Unlock for borrow
// ============================================
void handleBorrowAuthorized() {
  doorServo.unlock();
  lcd.printLines("Door Unlocked", "Take box & close");
  greenLed.on();
  changeState(State::BORROW_IN_PROGRESS);
}

// ============================================
// BORROW IN PROGRESS - Waiting for box removal
// ============================================
void handleBorrowInProgress() {
  if (!doorClosed) {
    if (!boxPresent) {
      Serial.println("Box removed!");
      lcd.printLines("Box Taken!", "Close door");
      changeState(State::BORROW_COMPLETING);
      return;
    } else {
      lcd.printLines("Door Open", "Take the box");
    }
  } else {
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

// ============================================
// BORROW COMPLETING - Box taken, wait for door close
// ============================================
void handleBorrowCompleting() {
  if (doorClosed && !boxPresent) {
    doorServo.lock();
    
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
    lcd.printLines("Take the box", "Then close door");
    changeState(State::BORROW_IN_PROGRESS);
  }
}

// ============================================
// RETURN AUTHORIZED - Unlock for return
// ============================================
void handleReturnAuthorized() {
  doorServo.unlock();
  lcd.printLines("Door Unlocked", "Place box inside");
  greenLed.off();
  changeState(State::RETURN_IN_PROGRESS);
}

// ============================================
// RETURN IN PROGRESS - Waiting for box placement
// ============================================
void handleReturnInProgress() {
  if (!doorClosed) {
    if (boxPresent) {
      Serial.println("Box placed in locker!");
      lcd.printLines("Box Detected!", "Close door");
      changeState(State::RETURN_COMPLETING);
      return;
    } else {
      lcd.printLines("Door Open", "Place box inside");
    }
  } else {
    lcd.printLines("Open Door", "Place box inside");
  }
  
  // Blink while waiting
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    static bool blinkState = false;
    blinkState = !blinkState;
    if (blinkState) greenLed.on(); else greenLed.off();
    lastBlink = millis();
  }
}

// ============================================
// RETURN COMPLETING - Box placed, wait for door close
// ============================================
void handleReturnCompleting() {
  if (doorClosed && boxPresent) {
    doorServo.lock();
    
    // Send return notification to Pi
    if (bluetooth.isConnected() && currentStudentID.length() > 0) {
      bluetooth.sendReturnNotification(currentStudentID);
      Serial.print("Return logged for: ");
      Serial.println(currentStudentID);
    }
    
    lcd.printLines("Return Complete", "Thank you!");
    greenLed.on();
    delay(3000);
    
    currentStudentID = "";
    changeState(State::IDLE_AVAILABLE);
    
  } else if (!doorClosed) {
    lcd.printLines("Close Door", "To complete");
  } else if (!boxPresent) {
    lcd.printLines("Place box", "Then close door");
    changeState(State::RETURN_IN_PROGRESS);
  }
}

// ============================================
// ERROR STATE
// ============================================
void handleError() {
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
    lastDisplayUpdate = 0;
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
    case State::AWAITING_RETURN_ID: return "AWAITING_RETURN_ID";
    case State::RETURN_AUTHORIZED:  return "RETURN_AUTHORIZED";
    case State::RETURN_IN_PROGRESS: return "RETURN_IN_PROGRESS";
    case State::RETURN_COMPLETING:  return "RETURN_COMPLETING";
    case State::ERROR:              return "ERROR";
    default:                        return "UNKNOWN";
  }
}

void updateSensors() {
  // Simple direct reading - the hardware debounce is enough
  bool newBoxPresent = boxSensor.isBoxPresent();
  bool newDoorClosed = doorSensor.isDoorClosed();
  
  if (newBoxPresent != boxPresent) {
    Serial.print("[SENSOR] Box: ");
    Serial.print(newBoxPresent ? "PRESENT" : "ABSENT");
    Serial.print(" (raw: ");
    Serial.print(boxSensor.readRaw());
    Serial.println(")");
    boxPresent = newBoxPresent;
  }
  
  if (newDoorClosed != doorClosed) {
    Serial.print("[SENSOR] Door: ");
    Serial.print(newDoorClosed ? "CLOSED" : "OPEN");
    Serial.print(" (raw: ");
    Serial.print(doorSensor.readRaw());
    Serial.println(")");
    doorClosed = newDoorClosed;
  }
}

void checkTimeouts() {
  unsigned long elapsed = millis() - stateEntryTime;
  
  switch (currentState) {
    case State::AWAITING_ID:
    case State::AWAITING_RETURN_ID:
      if (elapsed > INPUT_TIMEOUT_MS) {
        Serial.println("[TIMEOUT] ID entry timeout");
        lcd.printLines("Timeout", "Try again");
        delay(2000);
        inputBuffer = "";
        if (boxPresent) {
          changeState(State::IDLE_AVAILABLE);
        } else {
          changeState(State::IDLE_OCCUPIED);
        }
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
        
        if (boxPresent) {
          changeState(State::IDLE_AVAILABLE);
        } else {
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
        
        if (boxPresent) {
          // Return was successful even with timeout
          if (bluetooth.isConnected() && currentStudentID.length() > 0) {
            bluetooth.sendReturnNotification(currentStudentID);
          }
          currentStudentID = "";
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
