#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

enum class PacketType : uint8_t { Hello = 1, Command = 2, Status = 3 };
struct __attribute__((packed)) CarriagePacket {
  uint32_t magic;
  uint8_t version;
  PacketType type;
  uint8_t carriageId;
  uint32_t sequence;
  int16_t pwm;
  int8_t rssi;
};

WiFiUDP udp;
int16_t targetPwm = 0, appliedPwm = 0;
uint32_t lastCommandAt = 0, lastHelloAt = 0, lastRampAt = 0;
uint32_t lastSequence = 0;
bool udpStarted = false;

void writeMotor(int16_t pwm) {
  pwm = constrain(pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
  if (pwm > 0) {
    digitalWrite(MOTOR_IN1_PIN, HIGH);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    analogWrite(MOTOR_ENA_PIN, pwm);
  } else if (pwm < 0) {
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, HIGH);
    analogWrite(MOTOR_ENA_PIN, -pwm);
  } else {
    analogWrite(MOTOR_ENA_PIN, 0);
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
  }
}

void sendPacket(PacketType type) {
  if (WiFi.status() != WL_CONNECTED) return;
  CarriagePacket packet = {PACKET_MAGIC, PACKET_VERSION, type, CARRIAGE_ID, lastSequence,
                           appliedPwm, int8_t(WiFi.RSSI())};
  udp.beginPacket(CONTROLLER_IP, CARRIAGE_UDP_PORT);
  udp.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  udp.endPacket();
}

void updateMotor() {
  const uint32_t now = millis();
  if (now - lastCommandAt > COMMAND_TIMEOUT_MS) targetPwm = 0;
  if (now - lastRampAt < RAMP_INTERVAL_MS) return;
  lastRampAt = now;
  if ((appliedPwm > 0 && targetPwm < 0) || (appliedPwm < 0 && targetPwm > 0)) {
    appliedPwm = 0;
  } else if (appliedPwm < targetPwm) {
    appliedPwm += RAMP_STEP;
    if (appliedPwm > targetPwm) appliedPwm = targetPwm;
  } else if (appliedPwm > targetPwm) {
    appliedPwm -= RAMP_STEP;
    if (appliedPwm < targetPwm) appliedPwm = targetPwm;
  }
  writeMotor(appliedPwm);
}

void receiveCommands() {
  const int bytes = udp.parsePacket();
  if (bytes != int(sizeof(CarriagePacket))) return;
  CarriagePacket packet{};
  if (udp.read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet)) != int(sizeof(packet))) return;
  if (packet.magic != PACKET_MAGIC || packet.version != PACKET_VERSION ||
      packet.type != PacketType::Command || packet.carriageId != CARRIAGE_ID) return;
  lastSequence = packet.sequence;
  targetPwm = constrain(packet.pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
  lastCommandAt = millis();
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_ENA_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteRange(PWM_MAX);
  analogWriteFreq(PWM_FREQUENCY_HZ);
  writeMotor(0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[CARRIAGE 2] NodeMCU started; waiting for ESP32 Wi-Fi");
}

void loop() {
  const uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!udpStarted) {
      udpStarted = udp.begin(CARRIAGE_UDP_PORT) == 1;
      if (udpStarted) Serial.print("[CARRIAGE 2] Wi-Fi connected: ");
      if (udpStarted) Serial.println(WiFi.localIP());
    }
    receiveCommands();
    if (now - lastHelloAt >= HELLO_INTERVAL_MS) {
      lastHelloAt = now;
      sendPacket(PacketType::Status);
    }
  } else {
    targetPwm = 0;
    udpStarted = false;
    if (now - lastHelloAt >= 3000) {
      lastHelloAt = now;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
  updateMotor();
  yield();
}
