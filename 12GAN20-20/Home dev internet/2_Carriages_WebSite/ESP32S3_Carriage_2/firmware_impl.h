#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "carriage_radio.h"
#include "config.h"
#include "home_wifi_ota.h"

CarriageRadio carriageRadio;
HomeWifiOta homeWifi;
int16_t targetPwm = 0, appliedPwm = 0;
uint8_t targetServoPosition = 0, appliedServoPosition = 0;
LightMode targetLightMode = LightMode::Auto, appliedLightMode = LightMode::Auto;
uint32_t lastCommandAt = 0, lastStatusAt = 0, lastRampAt = 0;
uint32_t controllerSession = 0, lastSequence = 0;
uint8_t controllerMac[ESP_NOW_ETH_ALEN] = {};
bool controllerLinked = false, statusPending = false;
bool redOutputOn = false, lightStrobeActive = false;
uint32_t lastLightToggleAt = 0;

#include "functions/remote_hardware.h"

bool actuatorsReady = false;

void sendStatus() {
  const CarriagePacket packet = {ESPNOW_GROUP_ID, ESPNOW_PROTOCOL_VERSION,
                           controllerLinked ? PacketType::Status : PacketType::Hello,
                           REMOTE_CARRIAGE_ID,
                           controllerLinked ? controllerSession : 0,
                           controllerLinked ? lastSequence : 0,
                           appliedPwm, appliedServoPosition, appliedLightMode};
  carriageRadio.send(controllerLinked ? controllerMac : BROADCAST_MAC, packet);
}

void receiveCommand() {
  if (homeWifi.connecting() || homeWifi.updating()) return;
  RadioFrame frame{};
  if (!carriageRadio.receive(frame) || frame.broadcast ||
      millis() - frame.receivedAt > MAX_FRAME_AGE_MS) return;
  const CarriagePacket &packet = frame.packet;
  if (packet.group != ESPNOW_GROUP_ID || packet.version != ESPNOW_PROTOCOL_VERSION ||
      packet.type != PacketType::Command || packet.carriageId != REMOTE_CARRIAGE_ID ||
      packet.session == 0 || packet.sequence == 0) return;
  const bool sameController = controllerSession &&
      memcmp(controllerMac, frame.source, ESP_NOW_ETH_ALEN) == 0;
  const bool sameSession = sameController && packet.session == controllerSession;
  if (controllerLinked && !sameController) return;
  if ((!controllerLinked || !sameSession) &&
      (packet.pwm != 0 || packet.servoPosition != 0 || packet.lightMode != LightMode::Off)) return;
  if (sameSession && int32_t(packet.sequence - lastSequence) <= 0) return;

  if (!sameController) {
    if (controllerSession) carriageRadio.forget(controllerMac);
    memcpy(controllerMac, frame.source, ESP_NOW_ETH_ALEN);
  }
  if (!controllerLinked) Serial.println("[CARRIAGE 2] ESP-NOW controller connected");
  controllerLinked = true;
  controllerSession = packet.session;
  lastSequence = packet.sequence;
  targetPwm = constrain(packet.pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
  targetServoPosition = packet.servoPosition <= SERVO_PHYSICAL_MAX_ANGLE
      ? packet.servoPosition : 0;
  targetLightMode = uint8_t(packet.lightMode) <= uint8_t(LightMode::BlinkRed)
      ? packet.lightMode : LightMode::Off;
  lastCommandAt = frame.receivedAt;
  statusPending = true;
}

void prepareForOta() {
  targetPwm = 0;
  appliedPwm = 0;
  targetServoPosition = 0;
  targetLightMode = LightMode::Off;
  appliedServoPosition = 0;
  controllerLinked = false;
  writeMotor(0);
  writeServoAngle(0);
  updateLights(millis());
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
  pinMode(LIGHT_RED_PIN, OUTPUT);
  pinMode(LIGHT_GREEN_PIN, OUTPUT);
  if (!ledcAttachChannel(MOTOR_IN1_PIN, PWM_FREQUENCY_HZ, 10, 0) ||
      !ledcAttachChannel(MOTOR_IN2_PIN, PWM_FREQUENCY_HZ, 10, 1) ||
      !ledcAttachChannel(SERVO_SIGNAL_PIN, SERVO_FREQUENCY_HZ, 14, 2)) {
    Serial.println("[CARRIAGE 2] PWM initialization failed; motor disabled");
    return;
  }
  writeMotor(0);
  writeServoAngle(0);
  digitalWrite(LIGHT_RED_PIN, LOW);
  digitalWrite(LIGHT_GREEN_PIN, HIGH);

  if (!homeWifi.begin(prepareForOta) ||
      !carriageRadio.begin(WIFI_IF_STA, WIFI_CHANNEL)) {
    Serial.println("[CARRIAGE 2] ESP-NOW initialization failed; motor disabled");
    return;
  }
  actuatorsReady = true;
  homeWifi.service(true);
  Serial.printf("[CARRIAGE 2] ESP-NOW discovery on channel %u\n", unsigned(WIFI_CHANNEL));
}

void loop() {
  if (!actuatorsReady) { delay(10); return; }
  homeWifi.service(targetPwm == 0 && appliedPwm == 0);
  if (homeWifi.updating()) { delay(1); return; }
  if (homeWifi.connecting()) {
    targetPwm = 0;
    targetServoPosition = 0;
    targetLightMode = LightMode::Off;
    appliedPwm = 0;
    writeMotor(0);
    writeServoAngle(0);
    controllerLinked = false;
  }
  const uint32_t now = millis();
  if (controllerLinked && now - lastCommandAt >= COMMAND_TIMEOUT_MS) {
    controllerLinked = false;
    targetPwm = 0;
    targetServoPosition = 0;
    targetLightMode = LightMode::Off;
    statusPending = true;
    Serial.println("[CARRIAGE 2] ESP-NOW timeout; motor stopped");
  }
  receiveCommand();
  updateMotor();
  updateServo();
  updateLights(now);
  if (statusPending || now - lastStatusAt >= STATUS_INTERVAL_MS) {
    lastStatusAt = now;
    statusPending = false;
    sendStatus();
  }
  yield();
}
