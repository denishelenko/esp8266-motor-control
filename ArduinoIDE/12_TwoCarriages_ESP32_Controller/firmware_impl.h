#pragma once

#include <Arduino.h>
#include <WiFi.h>
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
  uint32_t sequence;
  int16_t pwm;
  int8_t rssi;
};

enum class MotionMode : uint8_t { Idle, Target, Program, Jog };
struct CarriageState {
  float position = 0.0f;
  float target = 0.0f;
  uint8_t speed = 50;
  uint8_t program = 0;
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
int8_t nodeRssi = -127;
bool carriagesLinked = false;

const char *modeName(MotionMode mode) {
  switch (mode) {
    case MotionMode::Target: return "ціль";
    case MotionMode::Program: return "програма";
    case MotionMode::Jog: return "ручний рух";
    default: return "стоп";
  }
}

bool nodeOnline() { return nodeKnown && millis() - nodeSeenAt < NODE_LINK_TIMEOUT_MS; }

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

void sendRemoteCommand() {
  if (!nodeOnline()) return;
  CarriagePacket packet = {PACKET_MAGIC, PACKET_VERSION, PacketType::Command, 2,
                           ++commandSequence, carriages[1].outputPwm, 0};
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
}

void setCarriageTarget(uint8_t index, float target, uint8_t speed) {
  CarriageState &c = carriages[index];
  c.target = constrain(target, -TRACK_LIMIT_M, TRACK_LIMIT_M);
  c.speed = constrain(speed, uint8_t(1), uint8_t(100));
  c.program = 0;
  c.mode = MotionMode::Target;
}

void stopCarriage(uint8_t index) {
  CarriageState &c = carriages[index];
  c.target = c.position;
  c.outputPwm = 0;
  c.program = 0;
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
    case 2: { const float levels[] = {0.35f, 0.7f, 1.1f, 1.45f, 1.8f};
              const float a = levels[step % 5]; c.target = (step / 5 & 1) ? -a : a; break; }
    case 3: { const float jumps[] = {-0.4f, 0.9f, -1.35f, 0.25f, 1.6f}; c.target = jumps[step % 5]; break; }
    default: c.target = (float(random(-180, 181)) / 100.0f); break;        // random
  }
  c.mode = MotionMode::Program;
}

void updateCarriage(uint8_t index) {
  CarriageState &c = carriages[index];
  const uint32_t now = millis();
  if (!c.lastTick) c.lastTick = now;
  const float dt = min(0.10f, (now - c.lastTick) / 1000.0f);
  c.lastTick = now;

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
  const float speedMps = ESTIMATED_SPEED_AT_100_PERCENT_MPS * c.speed / 100.0f;
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
    c["mode"] = modeName(carriages[i].mode);
    c["online"] = i == 0 || nodeOnline();
    c["rssi"] = i == 0 ? -45 : nodeRssi;
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
  } else if (!strcmp(type, "target")) {
    const float target = doc["position"] | 0.0f;
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) { setCarriageTarget(i, target, speed); });
  } else if (!strcmp(type, "program")) {
    const uint8_t program = constrain(doc["id"] | 1, 1, 4);
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) {
      carriages[i].speed = speed; carriages[i].program = program; carriages[i].programStep = 0;
      carriages[i].nextProgramAt = 0; selectNextProgramTarget(i);
    });
  } else if (!strcmp(type, "jog")) {
    const char *direction = doc["direction"] | "stop";
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) {
      CarriageState &c = carriages[i]; c.speed = speed; c.program = 0;
      if (!strcmp(direction, "left")) { c.target = -TRACK_LIMIT_M; c.mode = MotionMode::Jog; }
      else if (!strcmp(direction, "right")) { c.target = TRACK_LIMIT_M; c.mode = MotionMode::Jog; }
      else stopCarriage(i);
    });
  } else if (!strcmp(type, "stop")) {
    applyToSelected(selected, [&](uint8_t i) { stopCarriage(i); });
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
  pinMode(MOTOR_IN1_PIN, OUTPUT); pinMode(MOTOR_IN2_PIN, OUTPUT);
  ledcAttach(MOTOR_ENA_PIN, PWM_FREQUENCY_HZ, 10);
  writeLocalMotor(0);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  udp.begin(CARRIAGE_UDP_PORT);
  httpServer.on("/", HTTP_GET, [] { httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.begin();
  wsServer.begin(); wsServer.onEvent(onWebSocket);
  randomSeed(esp_random());
  Serial.printf("[ESP32] AP %s, site: http://%s\n", WIFI_SSID, AP_IP.toString().c_str());
}

void loop() {
  httpServer.handleClient();
  wsServer.loop();
  receiveNodePackets();
  updateCarriage(0); updateCarriage(1);
  writeLocalMotor(carriages[0].outputPwm);
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
