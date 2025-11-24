# Integration Guide: Smart Locker State Machine Implementation

## Quick Start

1. **Copy all your module files** (.h and .cpp files) to your Arduino project folder
2. **Replace the main SmartLocker.ino** with the provided state machine implementation
3. **Install required libraries** via Arduino Library Manager:
   - `Keypad` by Mark Stanley
   - `LiquidCrystal_I2C` by Frank de Brabander
   - `ESP32Servo` by Kevin Harrington
4. **Upload to your ESP32/TTGO**

## Key Integration Points

### 1. Hardware Configuration
All pin assignments are already configured in your `config_pins.h`. The state machine uses these definitions directly:

```cpp
// Your existing pin configuration is used throughout:
- LCD: I2C on pins 21(SDA), 22(SCL)
- Keypad: 4x4 matrix on configured pins
- IR Sensors: Analog pins 34 (door), 35 (box)
- Servo: Pin 26
- LEDs: Pins 1 (green), 3 (red)
- Buzzer: Pin 23
```

### 2. Module Integration
Your existing modules are integrated as follows:

| Module | Usage in State Machine |
|--------|----------------------|
| `lcd_display.*` | Status display, user prompts |
| `keypad.*` | ID input, command detection |
| `led.*` | Visual state feedback |
| `buzzer.*` | Audio alerts and confirmations |
| `servo.*` | Door lock/unlock control |
| `ir.*` | Physical security monitoring |
| `bt_comm.*` | Server communication |

### 3. State Machine Customization

#### Timing Adjustments
Edit these constants in `SmartLocker.ino`:

```cpp
constexpr unsigned long DOOR_OPEN_TIMEOUT_MS = 10000;  // Adjust door timeout
constexpr unsigned long INPUT_TIMEOUT_MS = 15000;      // Adjust input timeout
constexpr unsigned long AUTH_TIMEOUT_MS = 5000;        // Adjust auth timeout
```

#### ID Validation Rules
Modify the ID validation in `handleAwaitingID()`:

```cpp
if (inputBuffer.length() >= 6) { // Change minimum ID length
    // Add custom validation here
    if (isValidStudentID(inputBuffer)) {
        currentStudentID = inputBuffer;
        transitionToState(LockerState::AUTHENTICATING);
    }
}
```

#### Network Protocol
The current implementation sends/receives simple strings. Modify in `processCommStateMachine()`:

```cpp
// Current: Sends student ID, expects "OK" or "DENIED"
bluetooth.sendCode(currentStudentID);

// Customize for your protocol:
String request = formatAuthRequest(currentStudentID);
bluetooth.sendCode(request);
```

### 4. Testing Without Full Hardware

#### Sensor Simulation
Use the test harness to simulate sensors:

```cpp
#define SIMULATE_SENSORS  // Enable in test_harness.ino

// Then use serial commands:
// DO - Door Open
// DC - Door Close
// BO - Box Out
// BI - Box In
```

#### Bluetooth Simulation
Test without Raspberry Pi:

```cpp
// In processCommStateMachine(), add test mode:
#ifdef TEST_MODE
    authorizationResult = true;  // Always authorize in test
    authorizationPending = false;
#endif
```

### 5. Raspberry Pi Integration

The Pi should implement this simple protocol:

```python
# Pi receives (via Bluetooth):
student_id = bluetooth.read_line()

# Pi validates with cloud:
result = cloud_api.validate_student(student_id)

# Pi responds:
if result.authorized:
    bluetooth.write("OK\n")
else:
    bluetooth.write("DENIED\n")
```

### 6. Transaction Logging

The state machine logs all transactions. Enhance the `logTransaction()` function:

```cpp
void logTransaction(const String &type, const String &studentID) {
    // Current: Sends via Bluetooth
    String logEntry = type + "," + studentID + "," + String(millis());
    
    // Add: Local SD card logging
    #ifdef USE_SD_CARD
        sdCard.appendToFile("/logs.csv", logEntry);
    #endif
    
    // Add: Queue for offline storage
    #ifdef OFFLINE_QUEUE
        if (!bluetooth.isConnected()) {
            offlineQueue.push(logEntry);
        }
    #endif
}
```

### 7. Multi-Locker Network

To scale to multiple lockers:

```cpp
// Add locker ID to your config:
constexpr char LOCKER_ID[] = "L001";

// Include in all communications:
String request = String(LOCKER_ID) + ":" + studentID;

// Track locker status:
enum class NetworkStatus {
    STANDALONE,    // Single locker
    NETWORKED,     // Part of network
    COORDINATOR    // Network coordinator
};
```

### 8. Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| Door doesn't unlock | Check servo power (needs 5V, not 3.3V) |
| IR sensors always triggered | Adjust thresholds in `config_pins.h` |
| Bluetooth won't connect | Ensure Pi has paired with ESP32 |
| Keypad not responding | Check pull-up resistors on row pins |
| LCD shows garbage | Verify I2C address (usually 0x27 or 0x3F) |

### 9. Performance Optimization

For better response time:

```cpp
// Reduce debouncing if sensors are stable:
constexpr unsigned long SENSOR_DEBOUNCE_MS = 100;  // From 200ms

// Increase loop frequency by removing delays:
// Replace: delay(1000)
// With: non-blocking timer approach
```

### 10. Safety Enhancements

Add these safety features:

```cpp
// Emergency unlock (power failure):
void handlePowerFailure() {
    doorServo.unlock();  // Fail-safe: unlock on power loss
    // Requires capacitor backup for servo
}

// Tamper detection:
void checkTamper() {
    static int previousReading = 0;
    int vibration = analogRead(VIBRATION_SENSOR);
    if (abs(vibration - previousReading) > TAMPER_THRESHOLD) {
        logTransaction("TAMPER", "ALERT");
    }
}

// Temperature monitoring (prevent overheating):
void checkTemperature() {
    float temp = readTemperature();
    if (temp > MAX_OPERATING_TEMP) {
        transitionToState(LockerState::ERROR_STATE);
    }
}
```

## Deployment Checklist

- [ ] Test all state transitions with test harness
- [ ] Verify IR sensor thresholds with actual box
- [ ] Confirm Bluetooth pairing with Pi
- [ ] Test network failure scenarios
- [ ] Verify timeout values are appropriate
- [ ] Test emergency/maintenance procedures
- [ ] Document admin codes for maintenance mode
- [ ] Set up logging/monitoring system
- [ ] Create user instruction card
- [ ] Plan for power backup (if needed)

## Next Steps

1. **Start with standalone testing** using the test harness
2. **Add one feature at a time** (start with basic borrow/return)
3. **Integrate with Pi** once local testing is complete
4. **Deploy pilot** with single locker
5. **Scale to network** after pilot validation

## Support

For the concurrent state machine architecture:
- Review `STATE_MACHINE_ARCHITECTURE.md` for detailed design
- Use `test_harness.ino` for debugging
- Check state transitions in `state_diagram.puml`

Remember: The state machines are designed to be resilient. Even if one component fails, others continue operating, maintaining security and attempting recovery.
