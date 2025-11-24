// test_harness.ino
// Test harness for Smart Locker State Machine
// This file provides simulation and testing capabilities

#include "config_pins.h"

// ============================================
// TEST CONFIGURATION
// ============================================
#define ENABLE_SERIAL_COMMANDS  // Enable serial input for testing
#define SIMULATE_SENSORS        // Use serial commands to simulate sensor states
#define VERBOSE_LOGGING         // Extra debug output

// ============================================
// TEST SCENARIOS
// ============================================

class TestHarness {
private:
  bool _simulateDoorClosed = true;
  bool _simulateBoxPresent = true;
  bool _simulateNetworkOK = true;
  String _simulateAuthResponse = "OK";
  
public:
  // Sensor simulation
  bool getSimulatedDoorState() { return _simulateDoorClosed; }
  bool getSimulatedBoxState() { return _simulateBoxPresent; }
  
  // Network simulation
  bool getSimulatedNetworkState() { return _simulateNetworkOK; }
  String getSimulatedAuthResponse() { return _simulateAuthResponse; }
  
  // Test control functions
  void simulateDoorOpen() {
    _simulateDoorClosed = false;
    Serial.println("[TEST] Door opened");
  }
  
  void simulateDoorClose() {
    _simulateDoorClosed = true;
    Serial.println("[TEST] Door closed");
  }
  
  void simulateBoxRemoval() {
    _simulateBoxPresent = false;
    Serial.println("[TEST] Box removed");
  }
  
  void simulateBoxPlacement() {
    _simulateBoxPresent = true;
    Serial.println("[TEST] Box placed");
  }
  
  void simulateNetworkFailure() {
    _simulateNetworkOK = false;
    Serial.println("[TEST] Network failed");
  }
  
  void simulateNetworkRecovery() {
    _simulateNetworkOK = true;
    Serial.println("[TEST] Network recovered");
  }
  
  void simulateAuthDenied() {
    _simulateAuthResponse = "DENIED";
    Serial.println("[TEST] Auth will be denied");
  }
  
  void simulateAuthApproved() {
    _simulateAuthResponse = "OK";
    Serial.println("[TEST] Auth will be approved");
  }
  
  // Automated test scenarios
  void runScenario(int scenario) {
    Serial.print("[TEST] Running scenario ");
    Serial.println(scenario);
    
    switch(scenario) {
      case 1:
        testSuccessfulBorrow();
        break;
      case 2:
        testSuccessfulReturn();
        break;
      case 3:
        testBorrowTimeout();
        break;
      case 4:
        testAuthorizationDenied();
        break;
      case 5:
        testNetworkFailureDuringAuth();
        break;
      case 6:
        testForcedEntry();
        break;
      case 7:
        testSensorFailure();
        break;
      case 8:
        testCancelledBorrow();
        break;
      case 9:
        testOfflineReturn();
        break;
      case 10:
        testRapidStateChanges();
        break;
      default:
        Serial.println("[TEST] Unknown scenario");
    }
  }
  
private:
  void testSuccessfulBorrow() {
    Serial.println("[TEST SCENARIO 1] Successful Borrow");
    Serial.println("1. Press # to start");
    delay(1000);
    Serial.println("2. Enter ID: 123456#");
    delay(1000);
    Serial.println("3. Wait for auth...");
    delay(2000);
    Serial.println("4. Door unlocks");
    simulateDoorOpen();
    delay(1000);
    Serial.println("5. Remove box");
    simulateBoxRemoval();
    delay(1000);
    Serial.println("6. Close door");
    simulateDoorClose();
    delay(1000);
    Serial.println("[TEST] Scenario complete - Check if in IDLE_OCCUPIED state");
  }
  
  void testSuccessfulReturn() {
    Serial.println("[TEST SCENARIO 2] Successful Return");
    // Start from occupied state
    _simulateBoxPresent = false;
    Serial.println("1. Press # to return");
    delay(1000);
    Serial.println("2. Door unlocks");
    simulateDoorOpen();
    delay(1000);
    Serial.println("3. Place box");
    simulateBoxPlacement();
    delay(1000);
    Serial.println("4. Close door");
    simulateDoorClose();
    delay(1000);
    Serial.println("[TEST] Scenario complete - Check if in IDLE_AVAILABLE state");
  }
  
  void testBorrowTimeout() {
    Serial.println("[TEST SCENARIO 3] Borrow Timeout");
    Serial.println("1. Start borrow process");
    delay(1000);
    Serial.println("2. Door unlocks but user doesn't take box");
    simulateDoorOpen();
    delay(1000);
    Serial.println("3. Wait for timeout (10 seconds)...");
    delay(10500);
    Serial.println("[TEST] Should timeout and relock");
  }
  
  void testAuthorizationDenied() {
    Serial.println("[TEST SCENARIO 4] Authorization Denied");
    simulateAuthDenied();
    Serial.println("1. Press # to start");
    delay(1000);
    Serial.println("2. Enter ID: 999999#");
    delay(1000);
    Serial.println("3. Auth should be denied");
    delay(3000);
    Serial.println("[TEST] Should return to IDLE_AVAILABLE");
  }
  
  void testNetworkFailureDuringAuth() {
    Serial.println("[TEST SCENARIO 5] Network Failure During Auth");
    Serial.println("1. Start borrow");
    delay(1000);
    Serial.println("2. Enter ID");
    delay(1000);
    simulateNetworkFailure();
    Serial.println("3. Network fails during auth");
    delay(5500);
    Serial.println("[TEST] Should timeout and show offline mode");
  }
  
  void testForcedEntry() {
    Serial.println("[TEST SCENARIO 6] Forced Entry Detection");
    Serial.println("1. System in idle state");
    delay(1000);
    Serial.println("2. Force door open without authorization");
    simulateDoorOpen();
    delay(1000);
    Serial.println("[TEST] Should trigger security alarm");
    delay(6000);
    Serial.println("3. Close door to recover");
    simulateDoorClose();
  }
  
  void testSensorFailure() {
    Serial.println("[TEST SCENARIO 7] Sensor Failure");
    Serial.println("1. Simulate sensor returning invalid values");
    // This would need to be implemented in the sensor reading code
    Serial.println("[TEST] System should enter ERROR_STATE after repeated failures");
  }
  
  void testCancelledBorrow() {
    Serial.println("[TEST SCENARIO 8] Cancelled Borrow");
    Serial.println("1. Start borrow, get authorized");
    delay(2000);
    Serial.println("2. Open door");
    simulateDoorOpen();
    delay(1000);
    Serial.println("3. User changes mind, doesn't take box");
    delay(1000);
    Serial.println("4. Close door with box still inside");
    simulateDoorClose();
    delay(1000);
    Serial.println("[TEST] Should cancel and return to IDLE_AVAILABLE");
  }
  
  void testOfflineReturn() {
    Serial.println("[TEST SCENARIO 9] Offline Return");
    _simulateBoxPresent = false;
    simulateNetworkFailure();
    Serial.println("1. Network is down");
    delay(1000);
    Serial.println("2. User presses # to return");
    delay(1000);
    Serial.println("3. Should allow return without auth");
    simulateDoorOpen();
    delay(1000);
    simulateBoxPlacement();
    delay(1000);
    simulateDoorClose();
    Serial.println("[TEST] Return should complete offline");
  }
  
  void testRapidStateChanges() {
    Serial.println("[TEST SCENARIO 10] Rapid State Changes");
    Serial.println("1. Rapid door open/close");
    for(int i = 0; i < 5; i++) {
      simulateDoorOpen();
      delay(200);
      simulateDoorClose();
      delay(200);
    }
    Serial.println("2. Rapid box in/out");
    for(int i = 0; i < 5; i++) {
      simulateBoxRemoval();
      delay(200);
      simulateBoxPlacement();
      delay(200);
    }
    Serial.println("[TEST] System should remain stable");
  }
};

// ============================================
// SERIAL COMMAND INTERFACE
// ============================================

class SerialCommands {
private:
  TestHarness* _harness;
  String _commandBuffer;
  
public:
  SerialCommands(TestHarness* harness) : _harness(harness) {
    _commandBuffer.reserve(32);
  }
  
  void processCommands() {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        executeCommand(_commandBuffer);
        _commandBuffer = "";
      } else {
        _commandBuffer += c;
      }
    }
  }
  
private:
  void executeCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "") return;
    
    Serial.print("[CMD] ");
    Serial.println(cmd);
    
    // Sensor simulation commands
    if (cmd == "DOOR OPEN" || cmd == "DO") {
      _harness->simulateDoorOpen();
    }
    else if (cmd == "DOOR CLOSE" || cmd == "DC") {
      _harness->simulateDoorClose();
    }
    else if (cmd == "BOX OUT" || cmd == "BO") {
      _harness->simulateBoxRemoval();
    }
    else if (cmd == "BOX IN" || cmd == "BI") {
      _harness->simulateBoxPlacement();
    }
    
    // Network simulation commands
    else if (cmd == "NET FAIL" || cmd == "NF") {
      _harness->simulateNetworkFailure();
    }
    else if (cmd == "NET OK" || cmd == "NO") {
      _harness->simulateNetworkRecovery();
    }
    else if (cmd == "AUTH OK" || cmd == "AO") {
      _harness->simulateAuthApproved();
    }
    else if (cmd == "AUTH DENY" || cmd == "AD") {
      _harness->simulateAuthDenied();
    }
    
    // Keypad simulation
    else if (cmd.startsWith("KEY ")) {
      char key = cmd.charAt(4);
      Serial.print("[TEST] Simulating keypress: ");
      Serial.println(key);
      // This would need to be integrated with the keypad reading
    }
    
    // Run test scenarios
    else if (cmd.startsWith("TEST ")) {
      int scenario = cmd.substring(5).toInt();
      _harness->runScenario(scenario);
    }
    
    // State inspection
    else if (cmd == "STATUS" || cmd == "S") {
      printSystemStatus();
    }
    else if (cmd == "HELP" || cmd == "H") {
      printHelp();
    }
    else {
      Serial.println("[CMD] Unknown command. Type HELP for commands.");
    }
  }
  
  void printSystemStatus() {
    Serial.println("=== SYSTEM STATUS ===");
    Serial.print("Door: ");
    Serial.println(_harness->getSimulatedDoorState() ? "CLOSED" : "OPEN");
    Serial.print("Box: ");
    Serial.println(_harness->getSimulatedBoxState() ? "PRESENT" : "ABSENT");
    Serial.print("Network: ");
    Serial.println(_harness->getSimulatedNetworkState() ? "OK" : "FAILED");
    Serial.print("Auth Mode: ");
    Serial.println(_harness->getSimulatedAuthResponse());
    // Would also print current states of all state machines
    Serial.println("===================");
  }
  
  void printHelp() {
    Serial.println("=== TEST COMMANDS ===");
    Serial.println("Sensor Simulation:");
    Serial.println("  DOOR OPEN / DO    - Simulate door opening");
    Serial.println("  DOOR CLOSE / DC   - Simulate door closing");
    Serial.println("  BOX OUT / BO      - Simulate box removal");
    Serial.println("  BOX IN / BI       - Simulate box placement");
    Serial.println("");
    Serial.println("Network Simulation:");
    Serial.println("  NET FAIL / NF     - Simulate network failure");
    Serial.println("  NET OK / NO       - Simulate network recovery");
    Serial.println("  AUTH OK / AO      - Set auth to approve");
    Serial.println("  AUTH DENY / AD    - Set auth to deny");
    Serial.println("");
    Serial.println("Test Scenarios:");
    Serial.println("  TEST 1  - Successful borrow");
    Serial.println("  TEST 2  - Successful return");
    Serial.println("  TEST 3  - Borrow timeout");
    Serial.println("  TEST 4  - Authorization denied");
    Serial.println("  TEST 5  - Network failure during auth");
    Serial.println("  TEST 6  - Forced entry detection");
    Serial.println("  TEST 7  - Sensor failure");
    Serial.println("  TEST 8  - Cancelled borrow");
    Serial.println("  TEST 9  - Offline return");
    Serial.println("  TEST 10 - Rapid state changes");
    Serial.println("");
    Serial.println("Other:");
    Serial.println("  STATUS / S  - Show system status");
    Serial.println("  HELP / H    - Show this help");
    Serial.println("=====================");
  }
};

// ============================================
// INTEGRATION WITH MAIN CODE
// ============================================

#ifdef SIMULATE_SENSORS
  // When testing, replace the sensor readings in the main code with:
  // Instead of: doorSensor.isDoorClosed()
  // Use: testHarness.getSimulatedDoorState()
  
  // Instead of: boxSensor.isBoxPresent()
  // Use: testHarness.getSimulatedBoxState()
#endif

#ifdef ENABLE_SERIAL_COMMANDS
  // Add to global variables:
  // TestHarness testHarness;
  // SerialCommands serialCmd(&testHarness);
  
  // Add to setup():
  // Serial.println("Test mode enabled. Type HELP for commands.");
  
  // Add to loop():
  // serialCmd.processCommands();
#endif

// ============================================
// STATE MACHINE VALIDATION
// ============================================

class StateValidator {
public:
  // Validates state transitions are legal
  static bool validateTransition(int fromState, int toState) {
    // Define legal transitions matrix
    // This helps catch illegal state jumps during testing
    return true; // Implement validation logic
  }
  
  // Checks for stuck states
  static bool checkForStuckState(int currentState, unsigned long stateTime) {
    const unsigned long MAX_STATE_TIME = 60000; // 1 minute max per state
    if (stateTime > MAX_STATE_TIME) {
      Serial.print("[WARN] Stuck in state for ");
      Serial.print(stateTime / 1000);
      Serial.println(" seconds");
      return true;
    }
    return false;
  }
  
  // Validates sensor readings make sense
  static bool validateSensorCombination(bool doorClosed, bool boxPresent, int lockerState) {
    // Example: Can't have box present with door open in idle state
    // Add validation rules based on physical constraints
    return true;
  }
};

// ============================================
// PERFORMANCE MONITORING
// ============================================

class PerformanceMonitor {
private:
  unsigned long _loopCount = 0;
  unsigned long _lastReport = 0;
  unsigned long _maxLoopTime = 0;
  unsigned long _totalLoopTime = 0;
  
public:
  void recordLoop(unsigned long loopTime) {
    _loopCount++;
    _totalLoopTime += loopTime;
    if (loopTime > _maxLoopTime) {
      _maxLoopTime = loopTime;
    }
    
    // Report every 10 seconds
    if (millis() - _lastReport > 10000) {
      report();
      reset();
    }
  }
  
  void report() {
    Serial.println("=== PERFORMANCE ===");
    Serial.print("Loop count: ");
    Serial.println(_loopCount);
    Serial.print("Avg loop time: ");
    Serial.print(_totalLoopTime / _loopCount);
    Serial.println(" ms");
    Serial.print("Max loop time: ");
    Serial.print(_maxLoopTime);
    Serial.println(" ms");
    Serial.print("Loop rate: ");
    Serial.print(_loopCount / 10.0);
    Serial.println(" Hz");
    Serial.println("==================");
  }
  
  void reset() {
    _loopCount = 0;
    _maxLoopTime = 0;
    _totalLoopTime = 0;
    _lastReport = millis();
  }
};
