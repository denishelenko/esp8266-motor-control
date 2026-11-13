#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error Select ESP32S3 Dev Module in Arduino IDE.
#endif

// ESP32-S3 is the Wi-Fi access point and web controller.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";
constexpr uint8_t WIFI_CHANNEL = 6;  // Match the remote ESP-NOW channel.
constexpr uint32_t ESPNOW_GROUP_ID = 0x43525247UL;
// Local browser address advertised by the ESP32 access point.
// The site is available at http://motor-control.local/ after connecting to Wi-Fi.
constexpr char MDNS_HOSTNAME[] = "motor-control";
const IPAddress AP_IP(192, 168, 4, 1);
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;

// One motor per bridge. Both bridges receive the same direction and PWM.
constexpr uint8_t MOTOR_IN1_PIN = 1;      // GPIO1 -> A1A and B1A
constexpr uint8_t MOTOR_IN2_PIN = 2;      // GPIO2 -> A1B and B1B
constexpr bool MOTOR_DIRECTION_REVERSED = false;
constexpr uint8_t SERVO_SIGNAL_PIN = 42;  // GPIO42 -> TXS0108E A1; B1 -> servo signal.
constexpr uint8_t LIGHT_RED_PIN = 47;     // GPIO47 -> RGB R through resistor
constexpr uint8_t LIGHT_GREEN_PIN = 21;   // GPIO21 -> RGB G through resistor
constexpr uint8_t SERVO_DEFAULT_ANGLE = 45;  // Physical degrees; command is doubled.
constexpr uint8_t SERVO_COMMAND_PER_PHYSICAL_DEGREE = 2;
constexpr uint16_t SERVO_MIN_PULSE_US = 520;
constexpr uint16_t SERVO_MAX_PULSE_US = 2520;
constexpr uint16_t SERVO_FREQUENCY_HZ = 333;
// Measure the real 0->90 and 90->0 travel separately for each carriage.
// These initial estimates also drive the browser animation.
constexpr uint16_t SERVO_TRAVEL_90_MS[2] = {350, 350};
constexpr uint16_t SERVO_RETURN_90_MS[2] = {350, 350};
static_assert(SERVO_SIGNAL_PIN != MOTOR_IN1_PIN &&
              SERVO_SIGNAL_PIN != MOTOR_IN2_PIN &&
              LIGHT_RED_PIN != MOTOR_IN1_PIN && LIGHT_RED_PIN != MOTOR_IN2_PIN &&
              LIGHT_RED_PIN != SERVO_SIGNAL_PIN &&
              LIGHT_GREEN_PIN != MOTOR_IN1_PIN && LIGHT_GREEN_PIN != MOTOR_IN2_PIN &&
              LIGHT_GREEN_PIN != SERVO_SIGNAL_PIN && LIGHT_GREEN_PIN != LIGHT_RED_PIN,
              "ESP32-S3 output pins must be distinct");
constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;
constexpr uint32_t LIGHT_BLINK_HALF_PERIOD_MS = 125;

// A 20 m cable with 2 m reserve at each end leaves 16 m of working travel.
// The centre is 0 and the working range is -8..+8 m.
constexpr float TRACK_LENGTH_M = 20.0f;
constexpr float SAFE_MARGIN_M = 2.0f;
constexpr float TRACK_LIMIT_M = TRACK_LENGTH_M * 0.5f - SAFE_MARGIN_M;
constexpr float POSITION_TOLERANCE_M = 0.02f;
constexpr float UNCALIBRATED_SPEED_AT_100_PERCENT_MPS = 0.80f;  // Placeholder; not measured.
constexpr uint8_t TRAVEL_CALIBRATION_SPEED_PERCENT = 50;
struct TravelTimeCalibration {
  float right16mSeconds;
  float left16mSeconds;
};
// Fill in measured times at the speed above. Zero means not measured yet.
constexpr TravelTimeCalibration TRAVEL_TIMES[2] = {
  {0.0f, 0.0f},  // ESP32 carriage 1: right, left.
  {0.0f, 0.0f},  // ESP32-S3 carriage 2: right, left.
};
constexpr uint32_t COMMAND_INTERVAL_MS = 50;
constexpr uint32_t NODE_LINK_TIMEOUT_MS = 1200;
constexpr uint32_t NODE_ACK_TIMEOUT_MS = 500;
constexpr uint8_t NODE_MAX_ACK_LAG_PACKETS = 10;
constexpr uint32_t UI_BROADCAST_INTERVAL_MS = 70;
constexpr uint32_t JOG_TIMEOUT_MS = 650;
constexpr uint32_t LADDER_PAUSE_MS = 350;
constexpr uint32_t JERK_PAUSE_MS = 150;
constexpr uint32_t RANDOM_PAUSE_MS = 500;
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 30;
