#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// Must match the ESP32 controller settings.
constexpr char WIFI_SSID[] = "Motor-Control";
constexpr char WIFI_PASSWORD[] = "motor8266";
const IPAddress CONTROLLER_IP(192, 168, 4, 1);
constexpr uint16_t CARRIAGE_UDP_PORT = 4210;

// Carriage 2: NodeMCU v3 -> L298N channel A.
// Remove the ENA jumper before connecting D1.
constexpr uint8_t MOTOR_ENA_PIN = 5;   // D1 / GPIO5 -> ENA
constexpr uint8_t MOTOR_IN1_PIN = 4;   // D2 / GPIO4 -> IN1
constexpr uint8_t MOTOR_IN2_PIN = 14;  // D5 / GPIO14 -> IN2
constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

constexpr uint8_t CARRIAGE_ID = 2;
constexpr uint32_t PACKET_MAGIC = 0x43525247UL;  // "CRRG"
constexpr uint8_t PACKET_VERSION = 1;
constexpr uint32_t HELLO_INTERVAL_MS = 300;
constexpr uint32_t COMMAND_TIMEOUT_MS = 700;
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 30;
