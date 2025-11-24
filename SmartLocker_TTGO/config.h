#pragma once

// LCD (I2C)
constexpr int PIN_I2C_SDA       = 21;
constexpr int PIN_I2C_SCL       = 22;

// LEDs
constexpr int PIN_LED_GREEN     = 1;
constexpr int PIN_LED_RED       = 3;

// Buzzer
constexpr int PIN_BUZZER        = 23;

// Keypad 4x4
constexpr int PIN_KEYPAD_R0     = 14;
constexpr int PIN_KEYPAD_R1     = 12;
constexpr int PIN_KEYPAD_R2     = 13;
constexpr int PIN_KEYPAD_R3     = 15;

constexpr int PIN_KEYPAD_C0     = 2;
constexpr int PIN_KEYPAD_C1     = 0;
constexpr int PIN_KEYPAD_C2     = 25;
constexpr int PIN_KEYPAD_C3     = 4;

// IR sensors
constexpr int PIN_IR_SENSOR_BOX   = 35;
constexpr int IR_BOX_THRESHOLD      = 2800;

constexpr int PIN_IR_SENSOR_DOOR  = 34;
constexpr int IR_DOOR_THRESHOLD      = 2500;

// Servo
constexpr int PIN_SERVO_LOCK      = 26;
constexpr int DOOR_LOCKED_ANGLE    = 0;
constexpr int DOOR_UNLOCKED_ANGLE  = 90;

// System
constexpr unsigned long DOOR_OPEN_TIMEOUT_MS = 10000;  // 10 seconds to grab/return box
constexpr unsigned long INPUT_TIMEOUT_MS = 15000;      // 15 seconds for ID entry
constexpr unsigned long AUTH_TIMEOUT_MS = 5000;        // 5 seconds for auth response
constexpr unsigned long DISPLAY_MESSAGE_MS = 3000;     // 3 seconds for messages
constexpr unsigned long SENSOR_DEBOUNCE_MS = 200;      // Debounce for IR sensors
constexpr unsigned long BUZZER_BEEP_MS = 100;          // Short beep duration
constexpr unsigned long BUZZER_LONG_MS = 500;          // Long beep duration
