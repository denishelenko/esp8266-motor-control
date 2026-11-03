#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// ESP32-S3 is the Wi-Fi access point and web controller.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";
constexpr uint8_t WIFI_CHANNEL = 6;
const IPAddress AP_IP(192, 168, 4, 1);
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr uint16_t CARRIAGE_UDP_PORT = 4210;

// Carriage 1: motor bridge connected directly to ESP32-S3.
// GPIO4/5/6 are normal output pins on ESP32-S3 DevKitC-1.
constexpr uint8_t MOTOR_ENA_PIN = 4;
constexpr uint8_t MOTOR_IN1_PIN = 5;
constexpr uint8_t MOTOR_IN2_PIN = 6;
constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

constexpr float TRACK_LIMIT_M = 1.80f;
constexpr float POSITION_TOLERANCE_M = 0.02f;
constexpr float ESTIMATED_SPEED_AT_100_PERCENT_MPS = 0.80f;
constexpr uint32_t COMMAND_INTERVAL_MS = 50;
constexpr uint32_t NODE_LINK_TIMEOUT_MS = 1200;
constexpr uint32_t UI_BROADCAST_INTERVAL_MS = 70;

constexpr uint32_t PACKET_MAGIC = 0x43525247UL;  // "CRRG"
constexpr uint8_t PACKET_VERSION = 1;
