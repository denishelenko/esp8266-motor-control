#pragma once
#include <Arduino.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error Select ESP32S3 Dev Module in Arduino IDE.
#endif

// Must match the ESP-NOW group and AP channel of carriage 1.
constexpr uint8_t WIFI_CHANNEL = 6;
constexpr uint32_t ESPNOW_GROUP_ID = 0x43525247UL;

// One motor per bridge. Both bridges receive the same direction and PWM.
constexpr uint8_t MOTOR_IN1_PIN = 1;      // GPIO1 -> A1A and B1A
constexpr uint8_t MOTOR_IN2_PIN = 2;      // GPIO2 -> A1B and B1B
constexpr bool MOTOR_DIRECTION_REVERSED = false;
constexpr uint8_t SERVO_SIGNAL_PIN = 42;  // GPIO42 -> TXS0108E A1; B1 -> servo signal.
constexpr uint8_t LIGHT_RED_PIN = 47;     // GPIO47 -> RGB R through resistor
constexpr uint8_t LIGHT_GREEN_PIN = 21;   // GPIO21 -> RGB G through resistor
static_assert(SERVO_SIGNAL_PIN != MOTOR_IN1_PIN &&
              SERVO_SIGNAL_PIN != MOTOR_IN2_PIN &&
              LIGHT_RED_PIN != MOTOR_IN1_PIN && LIGHT_RED_PIN != MOTOR_IN2_PIN &&
              LIGHT_RED_PIN != SERVO_SIGNAL_PIN &&
              LIGHT_GREEN_PIN != MOTOR_IN1_PIN && LIGHT_GREEN_PIN != MOTOR_IN2_PIN &&
              LIGHT_GREEN_PIN != SERVO_SIGNAL_PIN && LIGHT_GREEN_PIN != LIGHT_RED_PIN,
              "ESP32-S3 output pins must be distinct");
constexpr uint8_t SERVO_CLOSED_ANGLE = 0;
constexpr uint8_t SERVO_PHYSICAL_MAX_ANGLE = 90;
constexpr uint8_t SERVO_COMMAND_PER_PHYSICAL_DEGREE = 2;
constexpr uint16_t SERVO_MIN_PULSE_US = 520;
constexpr uint16_t SERVO_MAX_PULSE_US = 2520;
constexpr uint16_t SERVO_FREQUENCY_HZ = 333;
constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;
constexpr uint32_t LIGHT_BLINK_HALF_PERIOD_MS = 125;

constexpr uint32_t STATUS_INTERVAL_MS = 300;
constexpr uint32_t COMMAND_TIMEOUT_MS = 700;
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 30;
