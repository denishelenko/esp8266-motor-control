#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "carriage_radio.h"
#include "config.h"
#include "web_ui.h"

enum class MotionMode : uint8_t { Idle, Target, Program, Jog };
struct CarriageState {
  float position = 0.0f;
  float target = 0.0f;
  uint8_t speed = 50;
  uint8_t program = 1;
  uint8_t servoAngle = SERVO_DEFAULT_ANGLE;
  uint8_t servoPosition = 0;
  bool servoEnabled = false;
  uint32_t nextServoAt = 0;
  int16_t outputPwm = 0;
  MotionMode mode = MotionMode::Idle;
  uint32_t lastTick = 0;
  uint32_t programStep = 0;
  uint32_t nextProgramAt = 0;
  bool ladderRight = true;
  uint32_t lastJogAt = 0;
};

WebServer httpServer(HTTP_PORT);
WebSocketsServer wsServer(WEBSOCKET_PORT);
CarriageRadio carriageRadio;
CarriageState carriages[2];
uint8_t nodeMac[ESP_NOW_ETH_ALEN] = {};
bool nodeKnown = false;
uint32_t nodeSeenAt = 0, commandSequence = 0, lastCommandSentAt = 0, lastBroadcastAt = 0;
uint32_t controllerSession = 0, lastAckSequence = 0, lastAckAt = 0, lastLocalRampAt = 0;
bool nodeAcked = false;
int8_t nodeRssi = -127;
uint8_t nodeServoPosition = 0;
int16_t nodeAppliedPwm = 0, localAppliedPwm = 0;
bool carriagesLinked = false;
bool controllerReady = false;
bool redOutputOn = false, lightStrobeActive = false;
uint32_t lastLightToggleAt = 0;

const char *modeName(MotionMode mode) {
  switch (mode) {
    case MotionMode::Target: return "ціль";
    case MotionMode::Program: return "програма";
    case MotionMode::Jog: return "ручний рух";
    default: return "стоп";
  }
}

bool nodeSeenRecently() { return nodeKnown && millis() - nodeSeenAt < NODE_LINK_TIMEOUT_MS; }
bool nodeOnline() {
  return nodeSeenRecently() && nodeAcked &&
         millis() - lastAckAt < NODE_ACK_TIMEOUT_MS &&
         commandSequence - lastAckSequence <= NODE_MAX_ACK_LAG_PACKETS;
}

void writeLocalMotor(int16_t pwm) {
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

void writeLights(bool redEnabled) {
  if (redOutputOn != redEnabled) {
    redOutputOn = redEnabled;
    digitalWrite(LIGHT_RED_PIN, redEnabled ? HIGH : LOW);
  }
}

void updateLights(uint32_t now) {
  if (localAppliedPwm == 0) {
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

void updateLocalMotor() {
  const uint32_t now = millis();
  const int16_t targetPwm = carriages[0].outputPwm;
  if (targetPwm == 0 && localAppliedPwm != 0) {
    localAppliedPwm = 0;
    lastLocalRampAt = now;
    writeLocalMotor(0);
    return;
  }
  if (localAppliedPwm == targetPwm) return;
  if (now - lastLocalRampAt < RAMP_INTERVAL_MS) return;
  lastLocalRampAt = now;
  if ((localAppliedPwm > 0 && targetPwm < 0) || (localAppliedPwm < 0 && targetPwm > 0)) {
    localAppliedPwm = 0;
  } else if (localAppliedPwm < targetPwm) {
    localAppliedPwm += RAMP_STEP;
    if (localAppliedPwm > targetPwm) localAppliedPwm = targetPwm;
  } else if (localAppliedPwm > targetPwm) {
    localAppliedPwm -= RAMP_STEP;
    if (localAppliedPwm < targetPwm) localAppliedPwm = targetPwm;
  }
  writeLocalMotor(localAppliedPwm);
}

void writeLocalServo(uint8_t physicalAngle) {
  const uint8_t commandAngle = constrain(uint16_t(physicalAngle) * 2, uint16_t(0), uint16_t(180));
  const uint16_t pulseUs = SERVO_MIN_PULSE_US +
      (uint32_t(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * commandAngle) / 180;
  const uint32_t duty = (uint64_t(pulseUs) * SERVO_FREQUENCY_HZ * 16384ULL + 500000ULL) / 1000000ULL;
  ledcWrite(SERVO_SIGNAL_PIN, duty);
}

void closeServo(CarriageState &c) {
  c.servoPosition = 0;
  c.nextServoAt = 0;
  c.servoEnabled = false;
}

void selectNextProgramTarget(uint8_t index);

void startProgram(uint8_t index) {
  CarriageState &c = carriages[index];
  if (c.program < 1 || c.program > 4) c.program = 1;
  c.programStep = 0;
  c.nextProgramAt = 0;
  c.ladderRight = true;
  c.mode = MotionMode::Program;
  if (!c.servoEnabled) {
    c.servoEnabled = true;
    c.servoPosition = 0;
    c.nextServoAt = millis() + SERVO_CLOSED_HOLD_MS;
  }
  selectNextProgramTarget(index);
}

void sendRemoteCommand() {
  // Discovery provides the remote MAC. Reconnect using a zero-output command.
  if (!nodeSeenRecently()) return;
  const bool confirmed = nodeOnline();
  if (++commandSequence == 0) ++commandSequence;
  const CarriagePacket packet = {ESPNOW_GROUP_ID, ESPNOW_PROTOCOL_VERSION,
                           PacketType::Command, REMOTE_CARRIAGE_ID,
                           controllerSession, commandSequence,
                           confirmed ? carriages[1].outputPwm : int16_t(0),
                           confirmed ? carriages[1].servoPosition : uint8_t(0)};
  carriageRadio.send(nodeMac, packet);
}

void receiveNodePacket() {
  RadioFrame frame{};
  if (!carriageRadio.receive(frame) || millis() - frame.receivedAt > MAX_FRAME_AGE_MS) return;
  const CarriagePacket &packet = frame.packet;
  if (packet.group != ESPNOW_GROUP_ID || packet.version != ESPNOW_PROTOCOL_VERSION ||
      packet.carriageId != REMOTE_CARRIAGE_ID) return;
  const bool hello = frame.broadcast && packet.type == PacketType::Hello;
  const bool status = !frame.broadcast && packet.type == PacketType::Status;
  if (!hello && !status) return;
  if (!nodeKnown || memcmp(nodeMac, frame.source, ESP_NOW_ETH_ALEN) != 0) {
    if (nodeSeenRecently()) return;
    if (nodeKnown) carriageRadio.forget(nodeMac);
    memcpy(nodeMac, frame.source, ESP_NOW_ETH_ALEN);
    nodeAcked = false;
    nodeAppliedPwm = 0;
    nodeServoPosition = 0;
  }
  nodeKnown = true;
  nodeSeenAt = frame.receivedAt;
  nodeRssi = frame.rssi;
  if (hello || packet.session != controllerSession || packet.sequence == 0) {
    nodeAcked = false;
    nodeAppliedPwm = 0;
    nodeServoPosition = 0;
    return;
  }
  const uint32_t lag = commandSequence - packet.sequence;
  if (lag > NODE_MAX_ACK_LAG_PACKETS ||
      (nodeAcked && int32_t(packet.sequence - lastAckSequence) < 0)) return;
  nodeAppliedPwm = packet.pwm;
  nodeServoPosition = packet.servoPosition;
  if (!nodeAcked || packet.sequence != lastAckSequence) {
    lastAckSequence = packet.sequence;
    lastAckAt = frame.receivedAt;
    nodeAcked = true;
  }
}

void setCarriageTarget(uint8_t index, float target, uint8_t speed) {
  CarriageState &c = carriages[index];
  c.target = constrain(target, -TRACK_LIMIT_M, TRACK_LIMIT_M);
  c.speed = constrain(speed, uint8_t(1), uint8_t(100));
  closeServo(c);
  c.mode = MotionMode::Target;
}

void stopCarriage(uint8_t index, bool keepServo = false) {
  CarriageState &c = carriages[index];
  c.target = c.position;
  c.outputPwm = 0;
  if (!keepServo) closeServo(c);
  c.mode = MotionMode::Idle;
}

template <typename Action>
void applyToSelected(uint8_t selected, Action action) {
  action(selected);
  if (carriagesLinked) action(selected == 0 ? 1 : 0);
}

void selectNextProgramTarget(uint8_t index) {
  CarriageState &c = carriages[index];
  const float l = TRACK_LIMIT_M;
  const uint32_t step = c.programStep++;
  switch (c.program) {
    case 1: c.target = (step & 1) ? -l : l; break; // маятник: край до краю
    case 2: { // драбинка: сусідня позначка через 2 м, зі зміною напрямку на краю
      if (c.position >= l - POSITION_TOLERANCE_M) c.ladderRight = false;
      if (c.position <= -l + POSITION_TOLERANCE_M) c.ladderRight = true;
      constexpr float spacing = 2.0f;
      const float nextMark = c.ladderRight
          ? floorf((c.position + POSITION_TOLERANCE_M) / spacing) + 1.0f
          : ceilf((c.position - POSITION_TOLERANCE_M) / spacing) - 1.0f;
      c.target = constrain(nextMark * spacing, -l, l);
      break;
    }
    case 3: { // ривки: короткі хаотичні стрибки на 1–3 м
      const float jump = 1.0f + float(random(0, 21)) / 10.0f;
      int direction = random(0, 2) ? 1 : -1;
      float candidate = c.position + direction * jump;
      if (candidate > l || candidate < -l) {
        direction = -direction;
        candidate = c.position + direction * jump;
      }
      c.target = constrain(candidate, -l, l);
      break;
    }
    default: { // рандом: довгі переходи, орієнтовно не менше 4 м
      const float distance = 4.0f + float(random(0, 31)) / 10.0f;
      int direction = random(0, 2) ? 1 : -1;
      float candidate = c.position + direction * distance;
      if (candidate > l || candidate < -l) {
        direction = -direction;
        candidate = c.position + direction * distance;
      }
      c.target = constrain(candidate, -l, l);
      break;
    }
  }
  c.mode = MotionMode::Program;
}

float estimatedSpeedMps(uint8_t index, bool right, uint8_t speedPercent) {
  const float measuredTime = right ? TRAVEL_TIMES[index].right16mSeconds
                                   : TRAVEL_TIMES[index].left16mSeconds;
  if (measuredTime > 0.0f) {
    const float measuredSpeed = (TRACK_LENGTH_M - 2.0f * SAFE_MARGIN_M) / measuredTime;
    // PWM versus real speed is only approximated as linear between measurements.
    return measuredSpeed * speedPercent / TRAVEL_CALIBRATION_SPEED_PERCENT;
  }
  return UNCALIBRATED_SPEED_AT_100_PERCENT_MPS * speedPercent / 100.0f;
}

void updateCarriage(uint8_t index) {
  CarriageState &c = carriages[index];
  const uint32_t now = millis();
  if (!c.lastTick) c.lastTick = now;
  const float dt = min(0.10f, (now - c.lastTick) / 1000.0f);
  c.lastTick = now;

  if (c.servoEnabled && c.nextServoAt && int32_t(now - c.nextServoAt) >= 0) {
    c.servoPosition = c.servoPosition ? 0 : c.servoAngle;
    c.nextServoAt = now + (c.servoPosition ? SERVO_OPEN_HOLD_MS : SERVO_CLOSED_HOLD_MS);
  }

  if (c.mode == MotionMode::Jog && now - c.lastJogAt >= JOG_TIMEOUT_MS) {
    stopCarriage(index, true);
  }
  if (c.mode == MotionMode::Program && fabsf(c.target - c.position) <= POSITION_TOLERANCE_M) {
    const uint32_t pauseMs = c.program == 1 ? 0 : c.program == 2 ? LADDER_PAUSE_MS
                                             : c.program == 3 ? JERK_PAUSE_MS : RANDOM_PAUSE_MS;
    if (pauseMs && !c.nextProgramAt) c.nextProgramAt = now + pauseMs;
    if (!pauseMs || int32_t(now - c.nextProgramAt) >= 0) {
      selectNextProgramTarget(index);
      c.nextProgramAt = 0;
    }
  }
  const float delta = c.target - c.position;
  if (c.mode == MotionMode::Idle || fabsf(delta) <= POSITION_TOLERANCE_M) {
    c.outputPwm = 0;
    if (c.mode == MotionMode::Target) c.mode = MotionMode::Idle;
    return;
  }
  const float direction = delta > 0 ? 1.0f : -1.0f;
  const uint8_t motionSpeed = c.mode == MotionMode::Program && c.program == 3 ? 100 : c.speed;
  const float speedMps = estimatedSpeedMps(index, direction > 0, motionSpeed);
  const float next = c.position + direction * speedMps * dt;
  c.position = direction > 0 ? min(next, c.target) : max(next, c.target);
  c.outputPwm = int16_t(direction * map(motionSpeed, 0, 100, 0, PWM_MAX));
}

void broadcastState() {
  JsonDocument doc;
  doc["type"] = "state";
  doc["linked"] = carriagesLinked;
  doc["nodeOnline"] = nodeOnline();
  JsonArray arr = doc["carriages"].to<JsonArray>();
  for (uint8_t i = 0; i < 2; ++i) {
    JsonObject c = arr.add<JsonObject>();
    c["id"] = i + 1;
    c["position"] = carriages[i].position;
    c["target"] = carriages[i].target;
    c["speed"] = carriages[i].speed;
    c["program"] = carriages[i].program;
    c["servoAngle"] = carriages[i].servoAngle;
    c["servoPosition"] = i == 0 ? carriages[i].servoPosition : (nodeOnline() ? nodeServoPosition : 0);
    c["servoEnabled"] = carriages[i].servoEnabled;
    c["servoTravel90Ms"] = SERVO_TRAVEL_90_MS[i];
    c["calibrated"] = TRAVEL_TIMES[i].right16mSeconds > 0 && TRAVEL_TIMES[i].left16mSeconds > 0;
    c["mode"] = modeName(carriages[i].mode);
    c["online"] = i == 0 || nodeOnline();
    c["rssi"] = i == 0 ? -45 : nodeRssi;
    c["outputPwm"] = i == 0 ? localAppliedPwm : (nodeOnline() ? nodeAppliedPwm : 0);
  }
  String message;
  serializeJson(doc, message);
  wsServer.broadcastTXT(message);
}

void handleUiCommand(const uint8_t *payload, size_t length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  const char *type = doc["type"] | "";
  const uint8_t selected = constrain(doc["carriage"] | 1, 1, 2) - 1;
  if (!strcmp(type, "link")) {
    carriagesLinked = nodeOnline() && (doc["enabled"] | false);
  } else if (!strcmp(type, "speed")) {
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) { carriages[i].speed = speed; });
  } else if (!strcmp(type, "target")) {
    const float target = doc["position"] | 0.0f;
    if (!isfinite(target)) return;
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) { setCarriageTarget(i, target, speed); });
  } else if (!strcmp(type, "program")) {
    const uint8_t program = constrain(doc["id"] | 1, 1, 4);
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) {
      carriages[i].speed = speed; carriages[i].program = program; startProgram(i);
    });
  } else if (!strcmp(type, "start")) {
    const uint8_t program = carriages[selected].program;
    const uint8_t speed = carriages[selected].speed;
    applyToSelected(selected, [&](uint8_t i) {
      carriages[i].program = program;
      carriages[i].speed = speed;
      startProgram(i);
    });
  } else if (!strcmp(type, "servo")) {
    const uint8_t angle = doc["angle"] | SERVO_DEFAULT_ANGLE;
    const bool enabled = doc["enabled"] | true;
    if (enabled && angle != 45 && angle != 90) return;
    applyToSelected(selected, [&](uint8_t i) {
      CarriageState &c = carriages[i];
      if (!enabled) { closeServo(c); return; }
      c.servoAngle = angle;
      if (!c.servoEnabled) {
        c.servoEnabled = true;
        c.servoPosition = 0;
        c.nextServoAt = millis() + SERVO_CLOSED_HOLD_MS;
      } else if (c.servoPosition) {
        c.servoPosition = angle;
      }
    });
  } else if (!strcmp(type, "jog")) {
    const char *direction = doc["direction"] | "stop";
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) {
      CarriageState &c = carriages[i]; c.speed = speed; closeServo(c);
      if (!strcmp(direction, "left") || !strcmp(direction, "right")) {
        c.target = !strcmp(direction, "left") ? -TRACK_LIMIT_M : TRACK_LIMIT_M;
        c.mode = MotionMode::Jog;
        c.lastJogAt = millis();
      } else if (c.mode == MotionMode::Jog) {
        stopCarriage(i);
      }
    });
  } else if (!strcmp(type, "stop")) {
    applyToSelected(selected, [&](uint8_t i) { stopCarriage(i, true); });
  } else if (!strcmp(type, "zero")) {
    applyToSelected(selected, [&](uint8_t i) { carriages[i].position = 0; carriages[i].target = 0; stopCarriage(i); });
  }
  broadcastState();
}

void onWebSocket(uint8_t, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) handleUiCommand(payload, length);
}

void setup() {
  Serial.begin(115200);
  controllerSession = esp_random();
  if (!controllerSession) controllerSession = 1;
  pinMode(MOTOR_IN1_PIN, OUTPUT); pinMode(MOTOR_IN2_PIN, OUTPUT);
  digitalWrite(MOTOR_IN1_PIN, LOW); digitalWrite(MOTOR_IN2_PIN, LOW);
  pinMode(LIGHT_RED_PIN, OUTPUT); pinMode(LIGHT_GREEN_PIN, OUTPUT);
  if (!ledcAttachChannel(MOTOR_IN1_PIN, PWM_FREQUENCY_HZ, 10, 0) ||
      !ledcAttachChannel(MOTOR_IN2_PIN, PWM_FREQUENCY_HZ, 10, 1) ||
      !ledcAttachChannel(SERVO_SIGNAL_PIN, SERVO_FREQUENCY_HZ, 14, 2)) {
    Serial.println("[CARRIAGE 1] PWM initialization failed; motor disabled");
    return;
  }
  writeLocalMotor(0);
  writeLocalServo(0);
  digitalWrite(LIGHT_RED_PIN, LOW);
  digitalWrite(LIGHT_GREEN_PIN, HIGH);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0)) ||
      !WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL)) {
    Serial.println("[CARRIAGE 1] Wi-Fi access point initialization failed");
    return;
  }
  const bool mdnsStarted = MDNS.begin(MDNS_HOSTNAME);
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    MDNS.addService("ws", "tcp", WEBSOCKET_PORT);
  }
  if (!carriageRadio.begin(WIFI_IF_AP, WIFI_CHANNEL)) {
    Serial.println("[CARRIAGE 1] ESP-NOW unavailable");
  }
  httpServer.on("/", HTTP_GET, [] { httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.begin();
  wsServer.begin(); wsServer.onEvent(onWebSocket);
  randomSeed(esp_random());
  controllerReady = true;
  Serial.printf("[ESP32] AP %s, site: http://%s.local/ (fallback http://%s/) mDNS: %s\n",
                WIFI_SSID, MDNS_HOSTNAME, AP_IP.toString().c_str(), mdnsStarted ? "OK" : "FAILED");
}

void loop() {
  if (!controllerReady) { delay(10); return; }
  httpServer.handleClient();
  wsServer.loop();
  receiveNodePacket();
  // Do not continue estimating a moving remote carriage while its actual
  // position is unknown; reconnecting must not restart an old command.
  if (!nodeOnline()) {
    if (carriages[1].mode != MotionMode::Idle || carriages[1].servoEnabled) stopCarriage(1);
    if (carriagesLinked) {
      stopCarriage(0);
      carriagesLinked = false;
    }
  }
  updateCarriage(0); updateCarriage(1);
  updateLocalMotor();
  updateLights(millis());
  writeLocalServo(carriages[0].servoPosition);
  const uint32_t now = millis();
  if (now - lastCommandSentAt >= COMMAND_INTERVAL_MS) {
    lastCommandSentAt = now;
    sendRemoteCommand();
  }
  if (now - lastBroadcastAt >= UI_BROADCAST_INTERVAL_MS) {
    lastBroadcastAt = now;
    broadcastState();
  }
}
