#pragma once
#include <Arduino.h>
#include "config.h"
#if defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR < 3
#error This sketch requires esp32 by Espressif Systems 3.x
#endif
using LocalWebServer = WebServer;
class LocalDnsServer {
 public:
  bool start(uint16_t, const String &, const IPAddress &) {
    // Captive DNS is optional; direct IP and motor.local remain available.
    return true;
  }
  void processNextRequest() {}
};
inline void disableWifiSleep() { WiFi.setSleep(false); }
inline void updateMdns() {}
inline void printBootInfo() {
  Serial.printf("\n[BOOT] ESP32-S3 reset=%d heap=%lu flash=%lu PSRAM=%lu\n",
    int(esp_reset_reason()), static_cast<unsigned long>(ESP.getFreeHeap()),
    static_cast<unsigned long>(ESP.getFlashChipSize()),
    static_cast<unsigned long>(ESP.getPsramSize()));
}
inline bool motorPwmBegin() {
  // ENA is permanently enabled by the L298N jumper. PWM is applied to the
  // active direction input instead, so no dedicated ENA GPIO is required.
  return ledcAttachChannel(MOTOR_IN1_PIN, PWM_FREQUENCY_HZ, 10, 0) &&
         ledcAttachChannel(MOTOR_IN2_PIN, PWM_FREQUENCY_HZ, 10, 1);
}
inline void motorPwmWrite(uint8_t pin, uint16_t duty) { ledcWrite(pin, duty); }
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
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <Servo.h>
using LocalWebServer = ESP8266WebServer;
using LocalDnsServer = DNSServer;
inline void disableWifiSleep() { WiFi.setSleepMode(WIFI_NONE_SLEEP); }
inline void updateMdns() { MDNS.update(); }
inline void printBootInfo() {
  Serial.printf("\n[BOOT] ESP8266 reset=%s heap=%u flash=%u\n",
    ESP.getResetReason().c_str(), ESP.getFreeHeap(), ESP.getFlashChipRealSize());
}
inline bool motorPwmBegin() {
  analogWriteRange(PWM_MAX); analogWriteFreq(PWM_FREQUENCY_HZ);
  pinMode(MOTOR_IN1_PIN, OUTPUT); pinMode(MOTOR_IN2_PIN, OUTPUT);
  return true;
}
inline void motorPwmWrite(uint8_t pin, uint16_t duty) { analogWrite(pin, duty); }
inline Servo &siteServo() { static Servo servo; return servo; }
inline bool servoWriteAngle(uint8_t angle) {
  Servo &servo = siteServo();
  if (!servo.attached()) servo.attach(SERVO_SIGNAL_PIN);
  if (!servo.attached()) return false;
  // Same API and signal generation as the known working myServo.write() test.
  servo.write(constrain(angle, 0, 180));
  return true;
}
inline void servoDetach() { if (siteServo().attached()) siteServo().detach(); }
#else
#error Unsupported board
#endif
