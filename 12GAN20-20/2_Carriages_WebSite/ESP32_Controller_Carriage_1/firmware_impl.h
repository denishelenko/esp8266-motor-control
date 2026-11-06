#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"
#include "web_ui.h"

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
};

WebServer httpServer(HTTP_PORT);
WebSocketsServer wsServer(WEBSOCKET_PORT);
WiFiUDP udp;
CarriageState carriages[2];
IPAddress nodeIp;
bool nodeKnown = false;
uint32_t nodeSeenAt = 0, commandSequence = 0, lastCommandSentAt = 0, lastBroadcastAt = 0;
uint32_t controllerSession = 0, lastAckSequence = 0, lastAckAt = 0, lastLocalRampAt = 0;
bool nodeAcked = false;
int8_t nodeRssi = -127;
uint8_t nodeServoPosition = 0;
int16_t nodeAppliedPwm = 0, localAppliedPwm = 0;
bool carriagesLinked = false;

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
  if (pwm > 0) {
    digitalWrite(MOTOR_IN1_PIN, HIGH);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    ledcWrite(MOTOR_ENA_PIN, pwm);
  } else if (pwm < 0) {
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, HIGH);
    ledcWrite(MOTOR_ENA_PIN, -pwm);
  } else {
    ledcWrite(MOTOR_ENA_PIN, 0);
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
  }
}

void updateLocalMotor() {
  const uint32_t now = millis();
  if (now - lastLocalRampAt < RAMP_INTERVAL_MS) return;
  lastLocalRampAt = now;
  const int16_t targetPwm = carriages[0].outputPwm;
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
  c.mode = MotionMode::Program;
  if (!c.servoEnabled) {
    c.servoEnabled = true;
    c.servoPosition = 0;
    c.nextServoAt = millis() + SERVO_CLOSED_HOLD_MS;
  }
  selectNextProgramTarget(index);
}

void sendRemoteCommand() {
  // A fresh status reveals the NodeMCU IP. Until its ACK is fresh, only
  // retransmit a zero-output command; never resume an old movement.
  if (!nodeSeenRecently()) return;
  const bool confirmed = nodeOnline();
  if (++commandSequence == 0) ++commandSequence;
  CarriagePacket packet = {PACKET_MAGIC, PACKET_VERSION, PacketType::Command, 2,
                           controllerSession, commandSequence,
                           confirmed ? carriages[1].outputPwm : int16_t(0), 0,
                           confirmed ? carriages[1].servoPosition : uint8_t(0)};
  udp.beginPacket(nodeIp, CARRIAGE_UDP_PORT);
  udp.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  udp.endPacket();
}

void receiveNodePackets() {
  const int bytes = udp.parsePacket();
  if (bytes != int(sizeof(CarriagePacket))) return;
  CarriagePacket packet{};
  if (udp.read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet)) != int(sizeof(packet))) return;
  if (packet.magic != PACKET_MAGIC || packet.version != PACKET_VERSION ||
      packet.carriageId != 2 || packet.type != PacketType::Status) return;
  nodeIp = udp.remoteIP();
  nodeKnown = true;
  nodeSeenAt = millis();
  nodeRssi = packet.rssi;
  if (packet.session != controllerSession || packet.sequence == 0) {
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
    lastAckAt = millis();
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
    case 1: c.target = (step & 1) ? -l : l; break;                         // pendulum
    case 2: { const float levels[] = {0.2f*l, 0.4f*l, 0.6f*l, 0.8f*l, l};
              const float a = levels[step % 5]; c.target = (step / 5 & 1) ? -a : a; break; }
    case 3: { const float jumps[] = {-0.25f*l, 0.5f*l, -0.75f*l, 0.1f*l, l}; c.target = jumps[step % 5]; break; }
    default: c.target = float(random(-int(l * 100), int(l * 100) + 1)) / 100.0f; break;
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

  if (c.mode == MotionMode::Program && now >= c.nextProgramAt) {
    if (fabsf(c.target - c.position) < POSITION_TOLERANCE_M) {
      selectNextProgramTarget(index);
      c.nextProgramAt = now + 250;
    }
  }
  const float delta = c.target - c.position;
  if (c.mode == MotionMode::Idle || fabsf(delta) < POSITION_TOLERANCE_M) {
    c.outputPwm = 0;
    if (c.mode == MotionMode::Target) c.mode = MotionMode::Idle;
    return;
  }
  const float direction = delta > 0 ? 1.0f : -1.0f;
  const float speedMps = estimatedSpeedMps(index, direction > 0, c.speed);
  const float next = c.position + direction * speedMps * dt;
  c.position = direction > 0 ? min(next, c.target) : max(next, c.target);
  c.outputPwm = int16_t(direction * map(c.speed, 0, 100, 0, PWM_MAX));
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
    carriagesLinked = doc["enabled"] | false;
  } else if (!strcmp(type, "speed")) {
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) { carriages[i].speed = speed; });
  } else if (!strcmp(type, "target")) {
    const float target = doc["position"] | 0.0f;
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
      if (!strcmp(direction, "left")) { c.target = -TRACK_LIMIT_M; c.mode = MotionMode::Jog; }
      else if (!strcmp(direction, "right")) { c.target = TRACK_LIMIT_M; c.mode = MotionMode::Jog; }
      else stopCarriage(i);
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
  ledcAttachChannel(MOTOR_ENA_PIN, PWM_FREQUENCY_HZ, 10, 0);
  ledcAttachChannel(SERVO_SIGNAL_PIN, SERVO_FREQUENCY_HZ, 14, 2);
  writeLocalMotor(0);
  writeLocalServo(0);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  const bool mdnsStarted = MDNS.begin(MDNS_HOSTNAME);
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    MDNS.addService("ws", "tcp", WEBSOCKET_PORT);
  }
  udp.begin(CARRIAGE_UDP_PORT);
  httpServer.on("/", HTTP_GET, [] { httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.begin();
  wsServer.begin(); wsServer.onEvent(onWebSocket);
  randomSeed(esp_random());
  Serial.printf("[ESP32] AP %s, site: http://%s.local/ (fallback http://%s/) mDNS: %s\n",
                WIFI_SSID, MDNS_HOSTNAME, AP_IP.toString().c_str(), mdnsStarted ? "OK" : "FAILED");
}

void loop() {
  httpServer.handleClient();
  wsServer.loop();
  receiveNodePackets();
  // Do not continue estimating a moving remote carriage while its actual
  // position is unknown; reconnecting must not restart an old command.
  if (!nodeOnline() && (carriages[1].mode != MotionMode::Idle || carriages[1].servoEnabled)) stopCarriage(1);
  if (carriagesLinked && !nodeOnline() && (carriages[0].mode != MotionMode::Idle || carriages[0].servoEnabled)) stopCarriage(0);
  updateCarriage(0); updateCarriage(1);
  updateLocalMotor();
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
