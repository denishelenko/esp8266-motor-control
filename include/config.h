#pragma once

#include <Arduino.h>

// Wi-Fi network created by the controller ESP8266.
// Change these two values before using the system in a public place.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor-8266";  // 8 characters minimum

constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr char CONTROLLER_IP[] = "192.168.4.1";

// Motor ESP8266 pins (NodeMCU labels D1 and D2).
// These are safe boot pins on common ESP-12E/NodeMCU boards.
constexpr uint8_t MOTOR_IN1_PIN = 5;  // GPIO5 / D1 -> MX1508 IN1
constexpr uint8_t MOTOR_IN2_PIN = 4;  // GPIO4 / D2 -> MX1508 IN2

constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

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
  {25, 0.16f, 0.15f},
  {40, 0.28f, 0.27f},
  {55, 0.42f, 0.40f},
  {70, 0.58f, 0.55f},
  {85, 0.73f, 0.69f},
  {100, 0.88f, 0.83f},
};
constexpr size_t SPEED_CALIBRATION_COUNT = sizeof(SPEED_CALIBRATION) / sizeof(SPEED_CALIBRATION[0]);

// Safety timings.
constexpr uint32_t JOG_DEADMAN_TIMEOUT_MS = 650;
constexpr uint32_t MOTOR_PACKET_TIMEOUT_MS = 1000;
constexpr uint32_t CONTROLLER_HEARTBEAT_MS = 200;
constexpr uint32_t MOTOR_STATUS_INTERVAL_MS = 500;

// Signed PWM is moved toward the target every 10 ms. This softens starts and
// forces direction changes to cross zero first.
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 24;
