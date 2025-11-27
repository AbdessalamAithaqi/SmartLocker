#pragma once

// ============================================
// PIN DEFINITIONS - ESP32 TTGO
// ============================================

// LCD (I2C)
constexpr int PIN_I2C_SDA       = 21;
constexpr int PIN_I2C_SCL       = 22;

// LED
constexpr int PIN_LED_GREEN     = 23;

// Keypad 4x4
constexpr int PIN_KEYPAD_R0     = 14;
constexpr int PIN_KEYPAD_R1     = 12;
constexpr int PIN_KEYPAD_R2     = 13;
constexpr int PIN_KEYPAD_R3     = 15;

constexpr int PIN_KEYPAD_C0     = 2;
constexpr int PIN_KEYPAD_C1     = 0;
constexpr int PIN_KEYPAD_C2     = 25;
constexpr int PIN_KEYPAD_C3     = 4;

// IR Sensors (Analog)
constexpr int PIN_IR_SENSOR_BOX   = 34;
constexpr int IR_BOX_THRESHOLD    = 2800;   // Adjust based on your sensor

constexpr int PIN_IR_SENSOR_DOOR  = 35;
constexpr int IR_DOOR_THRESHOLD   = 1300;   // Adjust based on your sensor

// Servo
constexpr int PIN_SERVO_LOCK      = 26;
constexpr int DOOR_LOCKED_ANGLE   = 0;      // Servo angle when locked
constexpr int DOOR_UNLOCKED_ANGLE = 90;     // Servo angle when unlocked

// ============================================
// TIMING CONSTANTS
// ============================================

// User interaction timeouts
constexpr unsigned long INPUT_TIMEOUT_MS       = 15000;  // 15 sec for ID entry
constexpr unsigned long AUTH_TIMEOUT_MS        = 10000;  // 10 sec for auth response
constexpr unsigned long DOOR_OPEN_TIMEOUT_MS   = 30000;  // 30 sec for borrow/return
constexpr unsigned long DISPLAY_MESSAGE_MS     = 3000;   // 3 sec for messages

// Sensor timing
constexpr unsigned long SENSOR_DEBOUNCE_MS     = 200;    // Sensor read interval

// ============================================
// VALIDATION
// ============================================

constexpr int MIN_STUDENT_ID_LENGTH = 8;   // Minimum digits for student ID
constexpr int MAX_STUDENT_ID_LENGTH = 9;   // Maximum digits for student ID

// ============================================
// BLUETOOTH
// ============================================
// Note: TTGO runs as Bluetooth SERVER
// The Raspberry Pi connects to us using rfcomm
// Device name is set in SmartLocker_TTGO.ino
