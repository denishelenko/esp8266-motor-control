#pragma once

#include <Arduino.h>

// Wi-Fi network created by the controller ESP8266.
// Change these two values before using the system in a public place.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";  // 8 characters minimum
constexpr uint8_t WIFI_CHANNEL = 6;

constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr char CONTROLLER_IP[] = "192.168.4.1";

// ESP8266 NodeMCU -> L298N (channel A).
// Remove the ENA jumper before connecting MOTOR_ENA_PIN.
constexpr uint8_t MOTOR_ENA_PIN = 14; // GPIO14 / D5 -> L298N ENA (PWM)
constexpr uint8_t MOTOR_IN1_PIN = 5;  // GPIO5 / D1  -> L298N IN1
constexpr uint8_t MOTOR_IN2_PIN = 4;  // GPIO4 / D2  -> L298N IN2

constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

constexpr uint8_t SPEED_MIN_PERCENT = 0;
constexpr uint8_t SPEED_MAX_PERCENT = 100;
constexpr uint8_t SPEED_DEFAULT_PERCENT = 50;

// Virtual track. Four metres total means centre 0 and physical ends -2..+2 m.
// SAFE_MARGIN_M keeps software targets away from the physical ends.
constexpr float TRACK_LENGTH_M = 4.0f;
constexpr float SAFE_MARGIN_M = 0.20f;
constexpr float POSITION_TOLERANCE_M = 0.015f;

// Replace these example values after measuring your actual mechanism.
// Each row is: PWM percent, speed to the right, speed to the left (metres/sec).
struct CalibrationPoint {
  uint8_t pwmPercent;
  float rightMetersPerSecond;
  float leftMetersPerSecond;
};

constexpr CalibrationPoint SPEED_CALIBRATION[] = {
  {0, 0.00f, 0.00f},
  {20, 0.12f, 0.11f},
  {40, 0.28f, 0.27f},
  {60, 0.47f, 0.45f},
  {80, 0.65f, 0.62f},
  {100, 0.88f, 0.83f},
};
constexpr size_t SPEED_CALIBRATION_COUNT = sizeof(SPEED_CALIBRATION) / sizeof(SPEED_CALIBRATION[0]);

// Safety timings.
constexpr uint32_t JOG_DEADMAN_TIMEOUT_MS = 650;
constexpr uint32_t MOTOR_PACKET_TIMEOUT_MS = 1000;
constexpr uint32_t MOTOR_LINK_TIMEOUT_MS = 500;
constexpr uint32_t ESPNOW_COMMAND_INTERVAL_MS = 20;
constexpr uint32_t MOTOR_STATUS_INTERVAL_MS = 100;
constexpr uint32_t ESPNOW_PAIR_INTERVAL_MS = 250;
constexpr uint32_t UI_BROADCAST_INTERVAL_MS = 50;

// Signed PWM is moved toward the target every 10 ms. This softens starts and
// forces direction changes to cross zero first.
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 24;
