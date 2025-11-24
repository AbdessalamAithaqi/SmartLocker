# Smart Locker Concurrent State Machine Architecture

## Overview
The Smart Locker system uses four concurrent state machines that operate independently but collaborate through shared state variables and events. This design ensures responsive user interaction, robust security monitoring, and reliable network communication.

## Concurrent State Machines

### 1. Main Locker State Machine (Primary Controller)
**Purpose:** Manages the overall locker workflow and coordinates other machines.

**States:**
- `IDLE_AVAILABLE` - Locker contains box, ready for borrowing
- `IDLE_OCCUPIED` - Box is borrowed, ready for return
- `AWAITING_ID` - Collecting student ID from keypad
- `AUTHENTICATING` - Waiting for authorization from server
- `BORROW_AUTHORIZED` - Authorization received, unlocking door
- `BORROW_IN_PROGRESS` - Door open, waiting for box removal
- `BORROW_COMPLETING` - Box removed, waiting for door close
- `RETURN_IN_PROGRESS` - Door open for return
- `RETURN_COMPLETING` - Box placed, waiting for door close
- `ERROR_STATE` - System error requiring intervention
- `MAINTENANCE` - Manual control mode for administrators

**Key Transitions:**
```
IDLE_AVAILABLE → [Press #] → AWAITING_ID
AWAITING_ID → [Valid ID entered] → AUTHENTICATING
AUTHENTICATING → [Authorized] → BORROW_AUTHORIZED
BORROW_AUTHORIZED → [Door opened] → BORROW_IN_PROGRESS
BORROW_IN_PROGRESS → [Box removed] → BORROW_COMPLETING
BORROW_COMPLETING → [Door closed] → IDLE_OCCUPIED

IDLE_OCCUPIED → [Press #] → RETURN_IN_PROGRESS
RETURN_IN_PROGRESS → [Box placed] → RETURN_COMPLETING
RETURN_COMPLETING → [Door closed] → IDLE_AVAILABLE
```

### 2. Security State Machine (Physical Monitoring)
**Purpose:** Continuously monitors physical sensors and maintains security state.

**States:**
- `DOOR_CLOSED_BOX_PRESENT` - Normal secured state with box
- `DOOR_CLOSED_BOX_ABSENT` - Normal secured state without box
- `DOOR_OPEN_BOX_PRESENT` - Door open with box inside
- `DOOR_OPEN_BOX_ABSENT` - Door open without box
- `SENSOR_ERROR` - Sensor malfunction detected

**Features:**
- Runs every loop iteration with debouncing (200ms)
- Triggers security alerts on unauthorized access
- Provides real-time state for other machines

### 3. Communication State Machine (Network Handler)
**Purpose:** Manages Bluetooth communication with Pi/Cloud asynchronously.

**States:**
- `IDLE` - No pending communication
- `CONNECTING` - Establishing Bluetooth connection
- `SENDING_AUTH` - Transmitting authorization request
- `WAITING_RESPONSE` - Awaiting server response
- `PROCESSING_RESPONSE` - Handling received response
- `OFFLINE_MODE` - Network unavailable, offline rules apply
- `ERROR` - Communication failure

**Features:**
- Non-blocking communication
- Automatic retry with exponential backoff
- Offline mode handling (allows returns, denies borrows)
- Transaction logging queue for network recovery

### 4. UI State Machine (User Interface)
**Purpose:** Manages display updates and user feedback independently.

**States:**
- `DISPLAY_STATUS` - Shows current locker status
- `INPUT_ACTIVE` - Managing keypad input
- `SHOWING_MESSAGE` - Temporary message display
- `ERROR_DISPLAY` - Error message with visual alerts

**Features:**
- Auto-refresh status every 5 seconds
- Message timeout handling (3 seconds)
- Visual feedback coordination (LEDs, buzzer)

## Inter-Machine Communication

### Shared State Variables
```cpp
// Cross-machine coordination
bool authorizationPending;     // Comm → Locker
bool authorizationResult;       // Comm → Locker
SecurityState currentSecurityState; // Security → All
bool isNetworkAvailable;        // Comm → Locker
String currentStudentID;         // Locker → Comm
```

### Event Flow Examples

#### Successful Borrow Sequence:
1. **User Action:** Press # (detected by Locker SM)
2. **Locker SM:** Transitions to AWAITING_ID, activates UI SM
3. **UI SM:** Updates display "Enter ID:"
4. **User Action:** Enters ID via keypad
5. **Locker SM:** Captures ID, transitions to AUTHENTICATING
6. **Comm SM:** Receives auth request, connects via Bluetooth
7. **Comm SM:** Sends ID to Pi, waits for response
8. **Server Response:** "OK" received
9. **Comm SM:** Sets authorizationResult = true
10. **Locker SM:** Transitions to BORROW_AUTHORIZED, unlocks door
11. **Security SM:** Detects DOOR_OPEN_BOX_PRESENT
12. **User Action:** Takes box
13. **Security SM:** Updates to DOOR_OPEN_BOX_ABSENT
14. **Locker SM:** Transitions to BORROW_COMPLETING
15. **User Action:** Closes door
16. **Security SM:** Updates to DOOR_CLOSED_BOX_ABSENT
17. **Locker SM:** Completes transaction, logs event

#### Return During Network Failure:
1. **Comm SM:** Detects network failure, sets isNetworkAvailable = false
2. **User Action:** Press # for return
3. **Locker SM:** Checks return request (always allowed offline)
4. **Locker SM:** Unlocks door without authentication
5. **Security SM:** Monitors box placement and door closure
6. **Locker SM:** Completes return, queues log for later transmission
7. **Comm SM:** Periodically retries connection for log upload

## Timing Configuration

| Parameter | Value | Purpose |
|-----------|-------|---------|
| DOOR_OPEN_TIMEOUT_MS | 10s | Maximum time door stays unlocked |
| INPUT_TIMEOUT_MS | 15s | Maximum time for ID entry |
| AUTH_TIMEOUT_MS | 5s | Maximum wait for server response |
| DISPLAY_MESSAGE_MS | 3s | Temporary message duration |
| SENSOR_DEBOUNCE_MS | 200ms | Sensor reading stabilization |

## Error Handling

### Sensor Failures
- Detected by repeated invalid readings (<100 analog value)
- Triggers ERROR_STATE after 10 consecutive failures
- Requires maintenance mode to reset

### Forced Entry Detection
- Security SM detects door open in IDLE states
- Triggers immediate alarm (buzzer + red LED)
- Logs SECURITY_BREACH event
- Attempts auto-recovery after door closure

### Network Failures
- Offline mode allows returns but denies borrows
- Transaction logs queued for retry
- Visual indication of offline status
- Automatic recovery on reconnection

## Maintenance Mode
Activated by pressing 'D' in ERROR_STATE:
- **A key:** Manual unlock
- **B key:** Manual lock  
- **C key:** View sensor readings
- **D key:** Exit maintenance mode

## Transaction Logging

Format: `TYPE,STUDENT_ID,TIMESTAMP`

Types:
- `BORROW` - Successful box checkout
- `RETURN` - Successful box return
- `DENIED` - Authorization denied
- `TIMEOUT` - Transaction timeout
- `SECURITY_BREACH` - Unauthorized access

## Benefits of Concurrent Design

1. **Responsiveness:** UI remains responsive during network delays
2. **Security:** Continuous monitoring prevents tampering
3. **Reliability:** Independent machines prevent cascade failures
4. **Maintainability:** Clear separation of concerns
5. **Scalability:** Easy to add new features to specific machines
6. **Debugging:** Isolated state machines simplify troubleshooting

## Testing Scenarios

### Normal Operation Tests:
1. Valid borrow with immediate box removal
2. Valid return with proper box placement
3. Invalid ID rejection
4. Timeout during ID entry
5. Timeout with door left open

### Edge Case Tests:
1. Network failure during authentication
2. Box removed then replaced during borrow
3. Door forced open in idle state
4. Sensor failure simulation
5. Rapid state transitions
6. Power loss recovery

### Stress Tests:
1. Rapid keypad entries
2. Repeated door open/close cycles
3. Bluetooth disconnection during auth
4. Multiple authorization attempts
5. Concurrent sensor state changes

## Future Enhancements

1. **Multi-Locker Coordination:** Add locker-to-locker communication
2. **Predictive Maintenance:** Track sensor degradation patterns
3. **Advanced Authentication:** Biometric or RFID support
4. **Remote Management:** Cloud-based monitoring dashboard
5. **Usage Analytics:** Pattern recognition for optimization
6. **Emergency Override:** Remote unlock capability
7. **Queue Management:** Reservation system for high demand
