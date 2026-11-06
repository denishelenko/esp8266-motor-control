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

// One autonomous device. GPIO numbers below are NOT physical header positions.
#if defined(PINOUT_ESP32_S3)
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error Select ESP32S3 Dev Module, not ESP32/ESP8266
#endif
constexpr char PINOUT_NAME[] = "ESP32-S3 N16R8";
constexpr char PINOUT_SERIAL_TEXT[] = "[PINS] GPIO5=ENA GPIO6=IN1 GPIO7=IN2 GPIO15=SERVO GPIO4=RED";
constexpr uint8_t MOTOR_ENA_PIN = 5;
constexpr uint8_t MOTOR_IN1_PIN = 6;
constexpr uint8_t MOTOR_IN2_PIN = 7;
constexpr uint8_t SERVO_SIGNAL_PIN = 15;
constexpr uint8_t LIGHT_RED_PIN = 4;
#elif defined(PINOUT_WEMOS_D1_MINI_PRO)
#if !defined(ESP8266)
#error Select Wemos D1 mini Pro (ESP8266)
#endif
constexpr char PINOUT_NAME[] = "Wemos D1 Mini Pro";
constexpr char PINOUT_SERIAL_TEXT[] = "[PINS] D1=ENA D2=IN1 D6=IN2 D5=SERVO D7=RED";
constexpr uint8_t MOTOR_ENA_PIN = 5;
constexpr uint8_t MOTOR_IN1_PIN = 4;
constexpr uint8_t MOTOR_IN2_PIN = 12;
constexpr uint8_t SERVO_SIGNAL_PIN = 14;
constexpr uint8_t LIGHT_RED_PIN = 13;
#else
#if !defined(ESP8266)
#error Select NodeMCU 1.0 (ESP-12E Module)
#endif
constexpr char PINOUT_NAME[] = "NodeMCU v3";
constexpr char PINOUT_SERIAL_TEXT[] = "[PINS] D1=ENA D2=IN1 D5=IN2 D6=SERVO D7=RED";
constexpr uint8_t MOTOR_ENA_PIN = 5;
constexpr uint8_t MOTOR_IN1_PIN = 4;
constexpr uint8_t MOTOR_IN2_PIN = 14;
constexpr uint8_t SERVO_SIGNAL_PIN = 12;
constexpr uint8_t LIGHT_RED_PIN = 13;
#endif

// SERVOMY SDS2601-26G: power red from a stable 5-8.4 V source and connect
// brown to the same GND as ESP8266 and L298N. ESP8266 uses Servo.h, matching
// the known working standalone myServo.write(angle) test.
// UI angles are the real, physical angles of this particular mechanism.
// Its electronics need a command twice as large: command 90° = physical 45°
// and command 180° = physical 90°.
constexpr uint8_t SERVO_CLOSED_ANGLE = 0;
constexpr uint8_t SERVO_PHYSICAL_MAX_ANGLE = 90;
constexpr uint8_t SERVO_COMMAND_PER_PHYSICAL_DEGREE = 2;
constexpr uint8_t SERVO_DEFAULT_OPEN_ANGLE = 45;
constexpr uint16_t SERVO_MIN_PULSE_US = 520;
constexpr uint16_t SERVO_MAX_PULSE_US = 2520;
constexpr uint16_t SERVO_FREQUENCY_HZ = 333;
// The servo is commanded directly to an endpoint. After reaching it, it stays
// open for 1 s and closed for 2 s. Actual travel speed is set by the servo.
constexpr uint32_t SERVO_OPEN_HOLD_MS = 1000;
constexpr uint32_t SERVO_CLOSED_HOLD_MS = 2000;

constexpr uint16_t PWM_MAX = 1023;
constexpr uint16_t PWM_FREQUENCY_HZ = 1000;

// The red LED strobes while a non-zero PWM value is actually applied to the
// motor. 125 ms per half-cycle gives four complete flashes per second.
constexpr uint32_t LIGHT_STROBE_HALF_PERIOD_MS = 125;

constexpr uint8_t SPEED_MIN_PERCENT = 1;
constexpr uint8_t SPEED_MAX_PERCENT = 100;
constexpr uint8_t SPEED_DEFAULT_PERCENT = 50;

// Working travel is 16 metres total: centre 0 and physical ends -8..+8 m.
// The 20 m rope keeps approximately 2 m outside the working area on each side.
// SAFE_MARGIN_M is an additional software reserve beyond the rope's 2 m reserve.
constexpr float TRACK_LENGTH_M = 16.0f;
// The 20 m rope has a 2 m reserve on both physical ends, leaving exactly
// 16 m of working travel: -8 m ... +8 m from the manually defined centre.
constexpr float SAFE_MARGIN_M = 0.0f;
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
  {20, 0.12f, 0.12f},
  {40, 0.28f, 0.28f},
  {60, 0.47f, 0.47f},
  {80, 0.65f, 0.65f},
  {100, 0.88f, 0.88f},
};
constexpr size_t SPEED_CALIBRATION_COUNT = sizeof(SPEED_CALIBRATION) / sizeof(SPEED_CALIBRATION[0]);

// Заповніть після тестів у ручному калібруванні. Відстань у метрах за
// фіксований час на вибраній швидкості; напрямки зберігаються окремо.
struct TimedCalibrationPoint {
  uint8_t seconds;
  uint8_t speedPercent;
  float rightMeters;
  float leftMeters;
};
constexpr TimedCalibrationPoint TIME_CALIBRATION[] = {
  {1, 50, 0.00f, 0.00f},
  {2, 50, 0.00f, 0.00f},
  {3, 50, 0.00f, 0.00f},
  {4, 50, 0.00f, 0.00f},
};
constexpr size_t TIME_CALIBRATION_COUNT = sizeof(TIME_CALIBRATION) / sizeof(TIME_CALIBRATION[0]);

// Safety timings.
constexpr uint32_t JOG_DEADMAN_TIMEOUT_MS = 650;
constexpr uint32_t UI_BROADCAST_INTERVAL_MS = 50;

// Signed PWM is moved toward the target every 10 ms. This softens starts and
// forces direction changes to cross zero first.
constexpr uint32_t UI_COMMAND_LEASE_MS = 2000;
constexpr uint32_t REVERSAL_PAUSE_MS = 50;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t RAMP_INTERVAL_MS = 10;
constexpr int16_t RAMP_STEP = 24;

constexpr bool safeOutputPin(uint8_t pin) {
#if defined(PINOUT_ESP32_S3)
  return pin == 4 || pin == 5 || pin == 6 || pin == 7 || pin == 15 || pin == 16;
#else
  return pin == 4 || pin == 5 || pin == 12 || pin == 13 || pin == 14 || pin == 16;
#endif
}
static_assert(safeOutputPin(MOTOR_ENA_PIN) && safeOutputPin(MOTOR_IN1_PIN) &&
              safeOutputPin(MOTOR_IN2_PIN), "Use D1/D2/D5/D6/D7, not boot/UART/flash pins");
static_assert(MOTOR_ENA_PIN != MOTOR_IN1_PIN && MOTOR_ENA_PIN != MOTOR_IN2_PIN &&
              MOTOR_IN1_PIN != MOTOR_IN2_PIN, "Motor pins must be distinct");
static_assert(safeOutputPin(SERVO_SIGNAL_PIN) && SERVO_SIGNAL_PIN != MOTOR_ENA_PIN &&
              SERVO_SIGNAL_PIN != MOTOR_IN1_PIN && SERVO_SIGNAL_PIN != MOTOR_IN2_PIN,
              "Servo pin must be safe and separate from motor");
static_assert(SERVO_PHYSICAL_MAX_ANGLE * SERVO_COMMAND_PER_PHYSICAL_DEGREE <= 180,
              "Servo physical-to-command ratio exceeds the command range");
static_assert(SERVO_FREQUENCY_HZ > 0 && 1000000UL / SERVO_FREQUENCY_HZ > SERVO_MAX_PULSE_US,
              "Servo pulse must fit inside its period");

static_assert(TRACK_LENGTH_M > 2 * SAFE_MARGIN_M && SAFE_MARGIN_M >= 0, "Invalid track");
static_assert(SPEED_MIN_PERCENT >= 1 && SPEED_MAX_PERCENT <= 100 &&
              SPEED_DEFAULT_PERCENT >= SPEED_MIN_PERCENT &&
              SPEED_DEFAULT_PERCENT <= SPEED_MAX_PERCENT, "Invalid speed range");

static_assert(safeOutputPin(LIGHT_RED_PIN) &&
              LIGHT_RED_PIN != MOTOR_ENA_PIN && LIGHT_RED_PIN != MOTOR_IN1_PIN &&
              LIGHT_RED_PIN != MOTOR_IN2_PIN && LIGHT_RED_PIN != SERVO_SIGNAL_PIN,
              "LED pin must not overlap motor/servo");
