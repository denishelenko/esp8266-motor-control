#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// Must match the ESP32 controller settings.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";
const IPAddress CONTROLLER_IP(192, 168, 4, 1);
constexpr uint16_t CARRIAGE_UDP_PORT = 4210;

// Carriage 2: ESP32-S3 -> L9110S.
// L9110S has no ENA pin. PWM is applied to the active IA/IB input.
constexpr uint8_t MOTOR_IN1_PIN = 6;       // GPIO6 -> L9110S IA
constexpr uint8_t MOTOR_IN2_PIN = 7;       // GPIO7 -> L9110S IB
constexpr uint8_t SERVO_SIGNAL_PIN = 15;   // GPIO15 -> yellow servo signal
constexpr uint8_t LIGHT_RED_PIN = 16;     // GPIO16 -> RGB R through resistor
constexpr uint8_t LIGHT_GREEN_PIN = 17;   // GPIO17 -> RGB G through resistor
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
constexpr uint32_t SERVO_OPEN_HOLD_MS = 2500;
constexpr uint32_t SERVO_CLOSED_HOLD_MS = 2500;
constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

constexpr uint8_t CARRIAGE_ID = 2;
constexpr uint32_t PACKET_MAGIC = 0x43525247UL;  // "CRRG"
constexpr uint8_t PACKET_VERSION = 3;
constexpr uint32_t HELLO_INTERVAL_MS = 300;
constexpr uint32_t COMMAND_TIMEOUT_MS = 700;
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 30;
