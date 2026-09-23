#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>
#include "config.h"

#if defined(DEVICE_ROLE_CONTROLLER) || defined(DEVICE_ROLE_SINGLE)
static float calibratedSpeed(uint8_t percent, bool right) {
  if (percent <= SPEED_CALIBRATION[0].pwmPercent)
    return right ? SPEED_CALIBRATION[0].rightMetersPerSecond : SPEED_CALIBRATION[0].leftMetersPerSecond;
  for (size_t i = 1; i < SPEED_CALIBRATION_COUNT; ++i) {
    if (percent <= SPEED_CALIBRATION[i].pwmPercent) {
      const CalibrationPoint &a = SPEED_CALIBRATION[i - 1], &b = SPEED_CALIBRATION[i];
      const float t = float(percent - a.pwmPercent) / float(b.pwmPercent - a.pwmPercent);
      const float av = right ? a.rightMetersPerSecond : a.leftMetersPerSecond;
      const float bv = right ? b.rightMetersPerSecond : b.leftMetersPerSecond;
      return av + (bv - av) * t;
    }
  }
  const CalibrationPoint &p = SPEED_CALIBRATION[SPEED_CALIBRATION_COUNT - 1];
  return right ? p.rightMetersPerSecond : p.leftMetersPerSecond;
}
#endif

#if defined(DEVICE_ROLE_CONTROLLER) || defined(DEVICE_ROLE_SINGLE)

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "web_ui.h"

ESP8266WebServer httpServer(HTTP_PORT);
WebSocketsServer wsServer(WEBSOCKET_PORT);
DNSServer dnsServer;

enum class Role : uint8_t { Unknown, Ui, Motor };
enum class Mode : uint8_t { Idle, Target, Jog, Program };
constexpr uint8_t MAX_CLIENTS = 8;
Role roles[MAX_CLIENTS] = {};
Mode mode = Mode::Idle;

float positionM = 0.0f, targetM = 0.0f;
String driveDirection = "stop";
uint8_t driveSpeed = 0, selectedSpeed = 55;
uint8_t activeProgram = 0, programStep = 0;
bool programWaiting = false;
uint32_t programResumeAt = 0, lastMotionAt = 0, lastBroadcastAt = 0, lastHeartbeatAt = 0, lastJogAt = 0;
uint32_t sequenceNumber = 0, lastMotorSeenAt = 0;
int16_t motorClient = -1;
int32_t motorRssi = 0;

constexpr float limitM() { return TRACK_LENGTH_M * 0.5f - SAFE_MARGIN_M; }
const char *modeName() {
  switch (mode) {
    case Mode::Target: return "рух до цілі";
    case Mode::Jog: return "ручний рух";
    case Mode::Program: return "програма";
    default: return "idle";
  }
}

#if defined(DEVICE_ROLE_SINGLE)
int16_t localTargetPwm = 0, localAppliedPwm = 0;
uint32_t lastRampAt = 0;
void writeLocalMotor(int16_t pwm) {
  analogWrite(MOTOR_IN1_PIN, pwm > 0 ? pwm : 0);
  analogWrite(MOTOR_IN2_PIN, pwm < 0 ? -pwm : 0);
}
void updateLocalRamp() {
  if (millis() - lastRampAt < RAMP_INTERVAL_MS) return;
  lastRampAt = millis();
  if (localAppliedPwm < localTargetPwm) {
    localAppliedPwm += RAMP_STEP;
    if (localAppliedPwm > localTargetPwm) localAppliedPwm = localTargetPwm;
  } else if (localAppliedPwm > localTargetPwm) {
    localAppliedPwm -= RAMP_STEP;
    if (localAppliedPwm < localTargetPwm) localAppliedPwm = localTargetPwm;
  }
  writeLocalMotor(localAppliedPwm);
}
#endif

bool motorOnline() {
#if defined(DEVICE_ROLE_SINGLE)
  return true;
#else
  return motorClient >= 0 && millis() - lastMotorSeenAt < 1800;
#endif
}

void setDrive(const char *direction, uint8_t speed) {
  speed = constrain(speed, 0, 100);
  if (strcmp(direction, "stop") == 0) speed = 0;
  if (driveDirection == direction && driveSpeed == speed) return;
  driveDirection = direction;
  driveSpeed = speed;
  ++sequenceNumber;
#if defined(DEVICE_ROLE_SINGLE)
  const int16_t pwm = map(speed, 0, 100, 0, PWM_MAX);
  localTargetPwm = driveDirection == "right" ? pwm : driveDirection == "left" ? -pwm : 0;
#endif
}

String stateJson() {
  JsonDocument d;
  d["type"] = "state"; d["seq"] = sequenceNumber;
  d["position"] = positionM; d["target"] = targetM;
  d["direction"] = driveDirection; d["speed"] = selectedSpeed; d["driveSpeed"] = driveSpeed;
  d["mode"] = modeName(); d["program"] = activeProgram; d["motorOnline"] = motorOnline();
  if (lastMotorSeenAt) d["rssi"] = motorRssi;
  String out; serializeJson(d, out); return out;
}

void broadcastState() {
  String message = stateJson();
  wsServer.broadcastTXT(message);
  lastBroadcastAt = millis();
}
void stopAll() {
  mode = Mode::Idle; activeProgram = 0; programWaiting = false; targetM = positionM;
  setDrive("stop", 0); broadcastState();
}

void chooseProgramTarget() {
  static const float p1[] = {-1.70f, 1.70f};
  static const float p2[] = {-0.45f, 0.45f, -0.90f, 0.90f, -1.35f, 1.35f, -1.70f, 1.70f};
  static const float p3[] = {-1.60f, 0.80f, 1.60f, -0.80f, 0.0f};
  if (activeProgram == 1) { targetM = p1[programStep % 2]; selectedSpeed = 45; }
  else if (activeProgram == 2) { targetM = p2[programStep % 8]; selectedSpeed = 55; }
  else if (activeProgram == 3) { targetM = p3[programStep % 5]; selectedSpeed = 72; }
  else { targetM = float(random(-170, 171)) / 100.0f; selectedSpeed = random(35, 86); }
  programWaiting = false;
}

void reachedTarget() {
  positionM = targetM; setDrive("stop", 0);
  if (mode == Mode::Program) {
    programWaiting = true; ++programStep;
    programResumeAt = millis() + (activeProgram == 4 ? random(180, 850) : 300);
  } else mode = Mode::Idle;
  broadcastState();
}

void updateMotion() {
  const uint32_t now = millis();
  if (!lastMotionAt) { lastMotionAt = now; return; }
  float dt = (now - lastMotionAt) / 1000.0f; lastMotionAt = now;
  if (dt > 0.10f) dt = 0.10f;
  if (mode == Mode::Jog && now - lastJogAt > JOG_DEADMAN_TIMEOUT_MS) { stopAll(); return; }
  if (mode == Mode::Program && programWaiting) {
    if ((int32_t)(now - programResumeAt) >= 0) chooseProgramTarget(); else return;
  }
  if (!motorOnline() && (mode == Mode::Target || mode == Mode::Program)) {
    setDrive("stop", 0);
    return;
  }
  if (mode == Mode::Target || mode == Mode::Program) {
    const float error = targetM - positionM;
    if (fabs(error) <= POSITION_TOLERANCE_M) { reachedTarget(); return; }
    const bool right = error > 0;
    const float step = calibratedSpeed(selectedSpeed, right) * dt;
    setDrive(right ? "right" : "left", selectedSpeed);
    if (step >= fabs(error)) { reachedTarget(); return; }
    positionM += right ? step : -step;
  } else if (mode == Mode::Jog && driveDirection != "stop") {
    const bool right = driveDirection == "right";
    positionM += (right ? 1.0f : -1.0f) * calibratedSpeed(driveSpeed, right) * dt;
    if (positionM >= limitM()) { positionM = limitM(); stopAll(); }
    if (positionM <= -limitM()) { positionM = -limitM(); stopAll(); }
  }
}

void handleWs(uint8_t client, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_DISCONNECTED) {
    if (client < MAX_CLIENTS) roles[client] = Role::Unknown;
    if (motorClient == client) { motorClient = -1; stopAll(); }
    return;
  }
  if (type == WStype_CONNECTED) {
    if (client < MAX_CLIENTS) roles[client] = Role::Unknown;
    String message = stateJson();
    wsServer.sendTXT(client, message); return;
  }
  if (type != WStype_TEXT) return;
  JsonDocument d;
  if (deserializeJson(d, payload, length) != DeserializationError::Ok) return;
  const char *t = d["type"] | "";
  if (!strcmp(t, "hello")) {
    const char *r = d["role"] | "";
    if (!strcmp(r, "motor")) {
#if !defined(DEVICE_ROLE_SINGLE)
      if (client < MAX_CLIENTS) roles[client] = Role::Motor;
      motorClient = client; lastMotorSeenAt = millis();
#endif
    } else if (!strcmp(r, "ui") && client < MAX_CLIENTS) roles[client] = Role::Ui;
    String message = stateJson();
    wsServer.sendTXT(client, message); broadcastState(); return;
  }
  const Role role = client < MAX_CLIENTS ? roles[client] : Role::Unknown;
#if !defined(DEVICE_ROLE_SINGLE)
  if (!strcmp(t, "status") && role == Role::Motor) {
    lastMotorSeenAt = millis(); motorRssi = d["rssi"] | 0; return;
  }
#endif
  if (role != Role::Ui) return;
  if (!strcmp(t, "stop")) { stopAll(); return; }
  if (!strcmp(t, "zero")) { stopAll(); positionM = targetM = 0; broadcastState(); return; }
  if (!strcmp(t, "target")) {
    activeProgram = 0; programWaiting = false; mode = Mode::Target;
    targetM = constrain(d["position"] | 0.0f, -limitM(), limitM());
    selectedSpeed = constrain(d["speed"] | 55, 25, 100); broadcastState(); return;
  }
  if (!strcmp(t, "program")) {
    const uint8_t id = constrain(d["id"] | 0, 1, 4);
    mode = Mode::Program; activeProgram = id; programStep = 0; chooseProgramTarget(); broadcastState(); return;
  }
  if (!strcmp(t, "jog")) {
    const char *dir = d["direction"] | "stop";
    if (!strcmp(dir, "stop")) { stopAll(); return; }
    mode = Mode::Jog; activeProgram = 0; lastJogAt = millis();
    selectedSpeed = constrain(d["speed"] | 40, 25, 100);
    setDrive(!strcmp(dir, "right") ? "right" : "left", selectedSpeed); return;
  }
}

void setup() {
  Serial.begin(115200); randomSeed(ESP.getCycleCount());
#if defined(DEVICE_ROLE_SINGLE)
  pinMode(MOTOR_IN1_PIN, OUTPUT); pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteRange(PWM_MAX); analogWriteFreq(PWM_FREQUENCY_HZ); writeLocalMotor(0);
#endif
  WiFi.persistent(false); WiFi.mode(WIFI_AP); WiFi.setOutputPower(20.5f);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 6, false, 4);
  dnsServer.start(53, "*", IPAddress(192,168,4,1)); MDNS.begin("motor");
  httpServer.on("/", [](){ httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.onNotFound([](){ httpServer.sendHeader("Location", "/"); httpServer.send(302, "text/plain", ""); });
  httpServer.begin(); wsServer.begin(); wsServer.onEvent(handleWs);
  Serial.println("Open http://motor.local or http://192.168.4.1");
}

void loop() {
  dnsServer.processNextRequest(); httpServer.handleClient(); wsServer.loop(); MDNS.update(); updateMotion();
#if defined(DEVICE_ROLE_SINGLE)
  updateLocalRamp();
#endif
  const uint32_t now = millis();
  if (now - lastBroadcastAt >= 150) broadcastState();
#if !defined(DEVICE_ROLE_SINGLE)
  if (motorClient >= 0 && now - lastHeartbeatAt >= CONTROLLER_HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    String message = stateJson();
    wsServer.sendTXT((uint8_t)motorClient, message);
  }
#endif
}

#elif defined(DEVICE_ROLE_MOTOR)

WebSocketsClient wsClient;
int16_t targetPwm = 0, appliedPwm = 0;
uint32_t lastPacketAt = 0, lastRampAt = 0, lastStatusAt = 0;
void writeMotor(int16_t pwm) {
  analogWrite(MOTOR_IN1_PIN, pwm > 0 ? pwm : 0);
  analogWrite(MOTOR_IN2_PIN, pwm < 0 ? -pwm : 0);
}
void emergencyStop() { targetPwm = appliedPwm = 0; writeMotor(0); }
void wsEvent(WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_CONNECTED) { wsClient.sendTXT("{\"type\":\"hello\",\"role\":\"motor\"}"); return; }
  if (type == WStype_DISCONNECTED) { emergencyStop(); return; }
  if (type != WStype_TEXT) return;
  JsonDocument d;
  if (deserializeJson(d, payload, length) != DeserializationError::Ok || strcmp(d["type"] | "", "state")) return;
  const char *dir = d["direction"] | "stop";
  const int speed = constrain(d["driveSpeed"] | 0, 0, 100);
  const int16_t pwm = map(speed, 0, 100, 0, PWM_MAX);
  targetPwm = !strcmp(dir, "right") ? pwm : !strcmp(dir, "left") ? -pwm : 0;
  lastPacketAt = millis();
}
void ramp() {
  if (millis() - lastRampAt < RAMP_INTERVAL_MS) return;
  lastRampAt = millis();
  if (appliedPwm < targetPwm) { appliedPwm += RAMP_STEP; if (appliedPwm > targetPwm) appliedPwm = targetPwm; }
  else if (appliedPwm > targetPwm) { appliedPwm -= RAMP_STEP; if (appliedPwm < targetPwm) appliedPwm = targetPwm; }
  writeMotor(appliedPwm);
}
void sendStatus() {
  JsonDocument d; d["type"] = "status"; d["rssi"] = WiFi.RSSI();
  String out; serializeJson(d, out); wsClient.sendTXT(out);
}
void setup() {
  Serial.begin(115200); pinMode(MOTOR_IN1_PIN, OUTPUT); pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteRange(PWM_MAX); analogWriteFreq(PWM_FREQUENCY_HZ); emergencyStop();
  WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.setOutputPower(20.5f);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); wsClient.begin(CONTROLLER_IP, WEBSOCKET_PORT, "/");
  wsClient.onEvent(wsEvent); wsClient.setReconnectInterval(1000); wsClient.enableHeartbeat(500, 1200, 2);
}
void loop() {
  wsClient.loop();
  if (millis() - lastPacketAt > MOTOR_PACKET_TIMEOUT_MS) emergencyStop();
  ramp();
  if (wsClient.isConnected() && millis() - lastStatusAt >= MOTOR_STATUS_INTERVAL_MS) {
    lastStatusAt = millis(); sendStatus();
  }
}

#else
#error Select controller, motor, or single environment
#endif


