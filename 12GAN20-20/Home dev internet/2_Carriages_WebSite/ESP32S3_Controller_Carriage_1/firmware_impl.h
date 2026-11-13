#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "carriage_radio.h"
#include "config.h"
#include "home_wifi_ota.h"
#include "web_ui.h"

#include "controller_state.h"

WebServer httpServer(HTTP_PORT);
WebSocketsServer wsServer(WEBSOCKET_PORT);
CarriageRadio carriageRadio;
HomeWifiOta homeWifi;
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

#include "controller_hardware.h"
#include "controller_estimator.h"
#include "controller_motion.h"

bool nodeSeenRecently() { return nodeKnown && millis() - nodeSeenAt < NODE_LINK_TIMEOUT_MS; }
bool nodeOnline() {
  return nodeSeenRecently() && nodeAcked &&
         millis() - lastAckAt < NODE_ACK_TIMEOUT_MS &&
         commandSequence - lastAckSequence <= NODE_MAX_ACK_LAG_PACKETS;
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
    // Estimated remaining time; zero calibration uses the placeholder in config.h.
    const uint8_t motionSpeed = carriages[i].mode == MotionMode::Program &&
                                carriages[i].program == 3 ? 100 : carriages[i].speed;
    c["estimatedTravelSeconds"] = estimatedTravelSeconds(
        i, carriages[i].position, carriages[i].target, motionSpeed);
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
    c["testRemainingMs"] = carriages[i].mode == MotionMode::TimedTest
        ? (millis() - carriages[i].testStartedAt < carriages[i].testDurationMs
            ? carriages[i].testDurationMs - (millis() - carriages[i].testStartedAt) : 0)
        : 0;
  }
  String message;
  serializeJson(doc, message);
  wsServer.broadcastTXT(message);
}

void handleUiCommand(const uint8_t *payload, size_t length) {
  if (homeWifi.connecting() || homeWifi.updating()) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  const char *type = doc["type"] | "";
  const uint8_t selected = constrain(doc["carriage"] | 1, 1, 2) - 1;
  if (!strcmp(type, "link")) {
    carriagesLinked = nodeOnline() && (doc["enabled"] | false);
  } else if (!strcmp(type, "speed")) {
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    applyToSelected(selected, [&](uint8_t i) { carriages[i].speed = speed; });
  } else if (!strcmp(type, "timed_test")) {
    // Physical distance is measured by the user; the browser never supplies metres.
    const char *direction = doc["direction"] | "";
    const int8_t sign = !strcmp(direction, "right") ? 1 : !strcmp(direction, "left") ? -1 : 0;
    const uint32_t durationMs = doc["durationMs"] | 0UL;
    if (!sign || (durationMs != 1000 && durationMs != 2000 && durationMs != 5000)) return;
    if (selected == 1 && !nodeOnline()) return;
    const uint8_t speed = constrain(doc["speed"] | 50, 1, 100);
    carriagesLinked = false;
    stopCarriage(0);
    stopCarriage(1);
    startTimedTest(selected, sign, speed, durationMs);
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

void prepareForOta() {
  stopCarriage(0);
  stopCarriage(1);
  carriagesLinked = false;
  localAppliedPwm = 0;
  writeLocalMotor(0);
  writeLocalServo(0);
  updateLights(millis());
  // The remote also has its own 700 ms command watchdog if this packet is lost.
  sendRemoteCommand();
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
  if (!homeWifi.begin(prepareForOta)) {
    Serial.println("[CARRIAGE 1] Wi-Fi station initialization failed");
    return;
  }
  if (!carriageRadio.begin(WIFI_IF_STA, WIFI_CHANNEL)) {
    Serial.println("[CARRIAGE 1] ESP-NOW unavailable");
  }
  httpServer.on("/", HTTP_GET, [] { httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.begin();
  wsServer.begin(); wsServer.onEvent(onWebSocket);
  randomSeed(esp_random());
  controllerReady = true;
  homeWifi.service(true);
  Serial.printf("[CARRIAGE 1] Home STA %s, site: http://%s.local/ or assigned IP\n",
                WIFI_SSID, MDNS_HOSTNAME);
}

void loop() {
  if (!controllerReady) { delay(10); return; }
  const bool idle = carriages[0].mode == MotionMode::Idle &&
                    carriages[1].mode == MotionMode::Idle &&
                    localAppliedPwm == 0 && nodeAppliedPwm == 0;
  homeWifi.service(idle);
  if (homeWifi.updating()) { delay(1); return; }
  if (homeWifi.connecting()) {
    stopCarriage(0);
    stopCarriage(1);
    carriagesLinked = false;
  }
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
