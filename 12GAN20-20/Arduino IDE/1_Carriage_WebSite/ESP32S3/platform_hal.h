#pragma once
#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR < 3
#error This sketch requires esp32 by Espressif Systems 3.x
#endif
inline void printBootInfo() {
  Serial.printf("\n[BOOT] ESP32-S3 reset=%d heap=%lu flash=%lu PSRAM=%lu\n",
    int(esp_reset_reason()), static_cast<unsigned long>(ESP.getFreeHeap()),
    static_cast<unsigned long>(ESP.getFlashChipSize()),
    static_cast<unsigned long>(ESP.getPsramSize()));
}
inline bool motorPwmBegin() {
  if (!ledcAttachChannel(MOTOR_IN1_PIN, PWM_FREQUENCY_HZ, 10, 0)) return false;
  if (ledcAttachChannel(MOTOR_IN2_PIN, PWM_FREQUENCY_HZ, 10, 1)) return true;
  ledcDetach(MOTOR_IN1_PIN);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  digitalWrite(MOTOR_IN1_PIN, LOW);
  return false;
}
inline bool &servoAttached() { static bool attached = false; return attached; }
inline bool servoPulse(uint32_t pulseUs) {
  if (!servoAttached()) {
    servoAttached() = ledcAttachChannel(SERVO_SIGNAL_PIN, SERVO_FREQUENCY_HZ, 14, 2);
    if (!servoAttached()) return false;
  }
  const uint32_t duty = (uint64_t(pulseUs) * SERVO_FREQUENCY_HZ * 16384ULL + 500000ULL) / 1000000ULL;
  return ledcWrite(SERVO_SIGNAL_PIN, duty);
}
inline bool servoWriteAngle(uint8_t angle) {
  const uint32_t pulseUs = map(constrain(angle, 0, 180), 0, 180,
                               SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  return servoPulse(pulseUs);
}
inline void servoDetach() {
  if (servoAttached()) ledcDetach(SERVO_SIGNAL_PIN);
  servoAttached() = false;
}
