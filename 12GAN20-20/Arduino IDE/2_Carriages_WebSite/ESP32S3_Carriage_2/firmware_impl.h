#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "carriage_radio.h"
#include "config.h"

CarriageRadio carriageRadio;
int16_t targetPwm = 0, appliedPwm = 0;
uint8_t targetServoPosition = 0, appliedServoPosition = 0;
uint32_t lastCommandAt = 0, lastStatusAt = 0, lastRampAt = 0;
uint32_t controllerSession = 0, lastSequence = 0;
uint8_t controllerMac[ESP_NOW_ETH_ALEN] = {};
bool controllerLinked = false, statusPending = false;
bool redOutputOn = false, lightStrobeActive = false;
uint32_t lastLightToggleAt = 0;

bool actuatorsReady = false;

void writeServoAngle(uint8_t physicalAngle) {
  physicalAngle = constrain(physicalAngle, SERVO_CLOSED_ANGLE, SERVO_PHYSICAL_MAX_ANGLE);
  const uint8_t commandAngle = physicalAngle * SERVO_COMMAND_PER_PHYSICAL_DEGREE;
  const uint32_t pulseUs = map(commandAngle, 0, 180, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  const uint32_t duty = (uint64_t(pulseUs) * SERVO_FREQUENCY_HZ * 16384ULL + 500000ULL) / 1000000ULL;
  ledcWrite(SERVO_SIGNAL_PIN, duty);
}

void writeLights(bool redEnabled) {
  if (redOutputOn != redEnabled) {
    redOutputOn = redEnabled;
    digitalWrite(LIGHT_RED_PIN, redEnabled ? HIGH : LOW);
  }
}

void updateLights(uint32_t now) {
  if (appliedPwm == 0) {
    lightStrobeActive = false;
    writeLights(false);
    digitalWrite(LIGHT_GREEN_PIN, HIGH);
    return;
  }
  digitalWrite(LIGHT_GREEN_PIN, LOW);
  if (!lightStrobeActive) {
    lightStrobeActive = true;
    lastLightToggleAt = now;
    writeLights(true);
    return;
  }
  if (now - lastLightToggleAt >= LIGHT_BLINK_HALF_PERIOD_MS) {
    lastLightToggleAt = now;
    writeLights(!redOutputOn);
  }
}

void writeMotor(int16_t pwm) {
  pwm = constrain(pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
  if (MOTOR_DIRECTION_REVERSED) pwm = -pwm;
  if (pwm > 0) {
    ledcWrite(MOTOR_IN2_PIN, 0);
    ledcWrite(MOTOR_IN1_PIN, pwm);
  } else if (pwm < 0) {
    ledcWrite(MOTOR_IN1_PIN, 0);
    ledcWrite(MOTOR_IN2_PIN, -pwm);
  } else {
    ledcWrite(MOTOR_IN1_PIN, 0);
    ledcWrite(MOTOR_IN2_PIN, 0);
  }
}

void sendStatus() {
  const CarriagePacket packet = {ESPNOW_GROUP_ID, ESPNOW_PROTOCOL_VERSION,
                           controllerLinked ? PacketType::Status : PacketType::Hello,
                           REMOTE_CARRIAGE_ID,
                           controllerLinked ? controllerSession : 0,
                           controllerLinked ? lastSequence : 0,
                           appliedPwm, appliedServoPosition};
  carriageRadio.send(controllerLinked ? controllerMac : BROADCAST_MAC, packet);
}

void updateMotor() {
  const uint32_t now = millis();
  if (targetPwm == 0 && appliedPwm != 0) {
    appliedPwm = 0;
    lastRampAt = now;
    writeMotor(0);
    return;
  }
  if (appliedPwm == targetPwm) return;
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

void updateServo() {
  if (appliedServoPosition == targetServoPosition) return;
  appliedServoPosition = targetServoPosition;
  writeServoAngle(appliedServoPosition);
}

void receiveCommand() {
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
  if ((!controllerLinked || !sameSession) && (packet.pwm != 0 || packet.servoPosition != 0)) return;
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
  targetServoPosition = (packet.servoPosition == 45 || packet.servoPosition == 90)
      ? packet.servoPosition : 0;
  lastCommandAt = frame.receivedAt;
  statusPending = true;
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

  WiFi.persistent(false);
  if (!WiFi.mode(WIFI_STA) || esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK ||
      esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      !carriageRadio.begin(WIFI_IF_STA, WIFI_CHANNEL)) {
    Serial.println("[CARRIAGE 2] ESP-NOW initialization failed; motor disabled");
    return;
  }
  actuatorsReady = true;
  Serial.printf("[CARRIAGE 2] ESP-NOW discovery on channel %u\n", unsigned(WIFI_CHANNEL));
}

void loop() {
  if (!actuatorsReady) { delay(10); return; }
  const uint32_t now = millis();
  if (controllerLinked && now - lastCommandAt >= COMMAND_TIMEOUT_MS) {
    controllerLinked = false;
    targetPwm = 0;
    targetServoPosition = 0;
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
