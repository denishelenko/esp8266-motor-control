#pragma once

#include <Arduino.h>

constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";
constexpr uint8_t WIFI_CHANNEL = 6;

constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr char CONTROLLER_IP[] = "192.168.4.1";

// One shared PWM output keeps both L298N channels at the same speed.
// Remove both ENA and ENB jumpers, then connect D1 to ENA and ENB together.
constexpr uint8_t MOTOR_ENA_PIN = 5;   // GPIO5  / D1 -> L298N ENA + ENB
constexpr uint8_t MOTOR_IN1_PIN = 4;   // GPIO4  / D2 -> L298N IN1 (motor A)
constexpr uint8_t MOTOR_IN2_PIN = 14;  // GPIO14 / D5 -> L298N IN2 (motor A)
constexpr uint8_t MOTOR_B_IN1_PIN = 12; // GPIO12 / D6 -> L298N IN3 (motor B)
constexpr uint8_t MOTOR_B_IN2_PIN = 13; // GPIO13 / D7 -> L298N IN4 (motor B)

constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

constexpr uint8_t SPEED_MIN_PERCENT = 1;
constexpr uint8_t SPEED_MAX_PERCENT = 100;
constexpr uint8_t SPEED_DEFAULT_PERCENT = 50;

constexpr float TRACK_LENGTH_M = 4.0f;
constexpr float SAFE_MARGIN_M = 0.20f;
constexpr float POSITION_TOLERANCE_M = 0.015f;

struct CalibrationPoint {
  uint8_t pwmPercent;
  float rightMetersPerSecond;
  float leftMetersPerSecond;
};

// Replace these example values after measuring the complete two-motor mechanism.
constexpr CalibrationPoint SPEED_CALIBRATION[] = {
  {0, 0.00f, 0.00f},
  {20, 0.12f, 0.12f},
  {40, 0.28f, 0.28f},
  {60, 0.47f, 0.47f},
  {80, 0.65f, 0.65f},
  {100, 0.88f, 0.88f},
};
constexpr size_t SPEED_CALIBRATION_COUNT = sizeof(SPEED_CALIBRATION) / sizeof(SPEED_CALIBRATION[0]);

constexpr uint32_t JOG_DEADMAN_TIMEOUT_MS = 650;
constexpr uint32_t MOTOR_PACKET_TIMEOUT_MS = 1000;
constexpr uint32_t MOTOR_LINK_TIMEOUT_MS = 500;
constexpr uint32_t ESPNOW_COMMAND_INTERVAL_MS = 20;
constexpr uint32_t MOTOR_STATUS_INTERVAL_MS = 100;
constexpr uint32_t ESPNOW_PAIR_INTERVAL_MS = 250;
constexpr uint32_t UI_BROADCAST_INTERVAL_MS = 50;

constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 24;
