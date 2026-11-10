#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

enum class PacketType : uint8_t { Hello = 1, Command = 2, Status = 3 };
struct __attribute__((packed)) CarriagePacket {
  uint32_t magic;
  uint8_t version;
  PacketType type;
  uint8_t carriageId;
  uint32_t session;
  uint32_t sequence;
  int16_t pwm;
  int8_t rssi;
  uint8_t servoPosition;
};
static_assert(sizeof(CarriagePacket) == 19, "Both boards must use the same UDP packet layout");

WiFiUDP udp;
int16_t targetPwm = 0, appliedPwm = 0;
uint8_t targetServoPosition = 0, appliedServoPosition = 0;
uint32_t lastCommandAt = 0, lastHelloAt = 0, lastRampAt = 0;
uint32_t controllerSession = 0, lastSequence = 0;
bool udpStarted = false, statusPending = false;
bool redOutputOn = false, lightStrobeActive = false;
uint32_t lastLightToggleAt = 0;

bool servoAttached = false;

bool writeServoAngle(uint8_t physicalAngle) {
  physicalAngle = constrain(physicalAngle, SERVO_CLOSED_ANGLE, SERVO_PHYSICAL_MAX_ANGLE);
  const uint8_t commandAngle = physicalAngle * SERVO_COMMAND_PER_PHYSICAL_DEGREE;
  const uint32_t pulseUs = map(commandAngle, 0, 180, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  const uint32_t duty = (uint64_t(pulseUs) * SERVO_FREQUENCY_HZ * 16384ULL + 500000ULL) / 1000000ULL;
  if (!servoAttached) {
    servoAttached = ledcAttachChannel(SERVO_SIGNAL_PIN, SERVO_FREQUENCY_HZ, 14, 2);
    if (!servoAttached) return false;
  }
  ledcWrite(SERVO_SIGNAL_PIN, duty);
  return true;
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
  if (now - lastLightToggleAt >= 125) {
    lastLightToggleAt = now;
    writeLights(!redOutputOn);
  }
}

void writeMotor(int16_t pwm) {
  pwm = constrain(pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
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

void sendPacket(PacketType type) {
  if (WiFi.status() != WL_CONNECTED) return;
  CarriagePacket packet = {PACKET_MAGIC, PACKET_VERSION, type, CARRIAGE_ID,
                           controllerSession, lastSequence,
                           appliedPwm, int8_t(WiFi.RSSI()), appliedServoPosition};
  udp.beginPacket(CONTROLLER_IP, CARRIAGE_UDP_PORT);
  udp.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  udp.endPacket();
}

void updateMotor() {
  const uint32_t now = millis();
  if (now - lastCommandAt > COMMAND_TIMEOUT_MS) {
    targetPwm = 0;
    targetServoPosition = 0;
  }
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

void receiveCommands() {
  const int bytes = udp.parsePacket();
  if (bytes != int(sizeof(CarriagePacket))) return;
  if (udp.remoteIP() != CONTROLLER_IP) return;
  CarriagePacket packet{};
  if (udp.read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet)) != int(sizeof(packet))) return;
  if (packet.magic != PACKET_MAGIC || packet.version != PACKET_VERSION ||
      packet.type != PacketType::Command || packet.carriageId != CARRIAGE_ID ||
      packet.session == 0 || packet.sequence == 0) return;
  if (packet.session != controllerSession) {
    // A new controller session must start with a stop command. This also
    // prevents a delayed command from an older session restarting the motor.
    if (packet.pwm != 0 || packet.servoPosition != 0) return;
    controllerSession = packet.session;
    lastSequence = 0;
    targetPwm = 0;
    targetServoPosition = 0;
  } else if (lastSequence && int32_t(packet.sequence - lastSequence) < 0) {
    return;  // A late UDP packet must not undo a newer command.
  }
  lastSequence = packet.sequence;
  targetPwm = constrain(packet.pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
  targetServoPosition = (packet.servoPosition == 45 || packet.servoPosition == 90)
      ? packet.servoPosition : 0;
  lastCommandAt = millis();
  statusPending = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  pinMode(LIGHT_RED_PIN, OUTPUT);
  pinMode(LIGHT_GREEN_PIN, OUTPUT);
  ledcAttachChannel(MOTOR_IN1_PIN, PWM_FREQUENCY_HZ, 10, 0);
  ledcAttachChannel(MOTOR_IN2_PIN, PWM_FREQUENCY_HZ, 10, 1);
  writeMotor(0);
  writeServoAngle(0);
  digitalWrite(LIGHT_RED_PIN, LOW);
  digitalWrite(LIGHT_GREEN_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[CARRIAGE 2] ESP32-S3 started; waiting for ESP32-S3 controller Wi-Fi");
}

void loop() {
  const uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!udpStarted) {
      udpStarted = udp.begin(CARRIAGE_UDP_PORT) == 1;
      if (udpStarted) Serial.print("[CARRIAGE 2] Wi-Fi connected: ");
      if (udpStarted) Serial.println(WiFi.localIP());
    }
    if (udpStarted) receiveCommands();
  } else {
    targetPwm = 0;
    targetServoPosition = 0;
    if (udpStarted) udp.stop();
    udpStarted = false;
    statusPending = false;
    controllerSession = 0;
    lastSequence = 0;
    if (now - lastHelloAt >= 3000) {
      lastHelloAt = now;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
  updateMotor();
  updateServo();
  updateLights(now);
  if (udpStarted && (statusPending || now - lastHelloAt >= HELLO_INTERVAL_MS)) {
    lastHelloAt = now;
    statusPending = false;
    sendPacket(PacketType::Status);  // ACK sequence plus current motor/servo telemetry.
  }
  yield();
}
