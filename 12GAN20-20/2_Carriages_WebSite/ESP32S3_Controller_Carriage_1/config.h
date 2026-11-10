#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// ESP32-S3 is the Wi-Fi access point and web controller.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";
constexpr uint8_t WIFI_CHANNEL = 6;
// Local browser address advertised by the ESP32 access point.
// The site is available at http://motor-control.local/ after connecting to Wi-Fi.
constexpr char MDNS_HOSTNAME[] = "motor-control";
const IPAddress AP_IP(192, 168, 4, 1);
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr uint16_t CARRIAGE_UDP_PORT = 4210;

// Carriage 1: L9110S connected directly to ESP32-S3.
// L9110S has no ENA pin; PWM is applied to IA/IB.
constexpr uint8_t MOTOR_IN1_PIN = 6;      // GPIO6 -> L9110S IA
constexpr uint8_t MOTOR_IN2_PIN = 7;      // GPIO7 -> L9110S IB
constexpr uint8_t SERVO_SIGNAL_PIN = 15;  // GPIO15 -> servo signal; shared GND.
constexpr uint8_t LIGHT_RED_PIN = 16;     // GPIO16 -> RGB R through resistor
constexpr uint8_t LIGHT_GREEN_PIN = 17;   // GPIO17 -> RGB G through resistor
constexpr uint8_t SERVO_DEFAULT_ANGLE = 45;  // Physical degrees; command is doubled.
constexpr uint16_t SERVO_MIN_PULSE_US = 520;
constexpr uint16_t SERVO_MAX_PULSE_US = 2520;
constexpr uint16_t SERVO_FREQUENCY_HZ = 333;
constexpr uint32_t SERVO_OPEN_HOLD_MS = 2500;
constexpr uint32_t SERVO_CLOSED_HOLD_MS = 2500;
constexpr uint16_t SERVO_TRAVEL_90_MS[2] = {350, 350};  // Visual estimates for carriages 1/2.
static_assert(SERVO_SIGNAL_PIN != MOTOR_IN1_PIN &&
              SERVO_SIGNAL_PIN != MOTOR_IN2_PIN &&
              LIGHT_RED_PIN != MOTOR_IN1_PIN && LIGHT_RED_PIN != MOTOR_IN2_PIN &&
              LIGHT_RED_PIN != SERVO_SIGNAL_PIN &&
              LIGHT_GREEN_PIN != MOTOR_IN1_PIN && LIGHT_GREEN_PIN != MOTOR_IN2_PIN &&
              LIGHT_GREEN_PIN != SERVO_SIGNAL_PIN && LIGHT_GREEN_PIN != LIGHT_RED_PIN,
              "ESP32-S3 output pins must be distinct");
constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

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
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 30;

constexpr uint32_t PACKET_MAGIC = 0x43525247UL;  // "CRRG"
constexpr uint8_t PACKET_VERSION = 3;
