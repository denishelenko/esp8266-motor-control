#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"

#if defined(DEVICE_ROLE_CONTROLLER) || defined(DEVICE_ROLE_SINGLE)
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#endif

#if defined(DEVICE_ROLE_CONTROLLER) || defined(DEVICE_ROLE_MOTOR)
extern "C" {
#include <espnow.h>
#include <user_interface.h>
}

constexpr uint32_t ESPNOW_MAGIC = 0x4D4F544FUL;  // "MOTO"
constexpr uint8_t ESPNOW_PROTOCOL_VERSION = 1;
enum class EspNowType : uint8_t { Hello = 1, PairAck = 2, Command = 3, Status = 4 };

struct __attribute__((packed)) EspNowHeader {
  uint32_t magic;
  uint8_t version;
  EspNowType type;
};
struct __attribute__((packed)) EspNowPairPacket {
  EspNowHeader header;
  uint8_t targetMac[6];
};
struct __attribute__((packed)) EspNowCommandPacket {
  EspNowHeader header;
  uint32_t sequence;
  int16_t pwm;
};
struct __attribute__((packed)) EspNowStatusPacket {
  EspNowHeader header;
  uint32_t acknowledgedSequence;
  int16_t appliedPwm;
  uint8_t failsafeActive;
};

constexpr uint8_t ESPNOW_MAX_PACKET_SIZE = sizeof(EspNowStatusPacket);
const uint8_t ESPNOW_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool validEspNowHeader(const EspNowHeader &header, EspNowType type) {
  return header.magic == ESPNOW_MAGIC && header.version == ESPNOW_PROTOCOL_VERSION && header.type == type;
}

bool addEncryptedPeer(const uint8_t *mac) {
  uint8_t *peer = const_cast<uint8_t *>(mac);
  if (esp_now_is_peer_exist(peer)) return true;
  return esp_now_add_peer(peer, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL,
                          const_cast<uint8_t *>(ESPNOW_LMK), sizeof(ESPNOW_LMK)) == 0;
}

bool initEspNow(void (*receiveCallback)(uint8_t *, uint8_t *, uint8_t)) {
  if (esp_now_init() != 0) return false;
  if (esp_now_set_self_role(ESP_NOW_ROLE_COMBO) != 0) return false;
  esp_now_set_kok(const_cast<uint8_t *>(ESPNOW_KOK), sizeof(ESPNOW_KOK));
  esp_now_register_recv_cb(receiveCallback);
  uint8_t *broadcast = const_cast<uint8_t *>(ESPNOW_BROADCAST_MAC);
  if (!esp_now_is_peer_exist(broadcast))
    esp_now_add_peer(broadcast, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, nullptr, 0);
  return true;
}
#endif

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

enum class Role : uint8_t { Unknown, Ui };
enum class Mode : uint8_t { Idle, Target, Jog, Program };
constexpr uint8_t MAX_CLIENTS = 8;
Role roles[MAX_CLIENTS] = {};
Mode mode = Mode::Idle;

float positionM = 0.0f, targetM = 0.0f;
String driveDirection = "stop";
uint8_t driveSpeed = 0, selectedSpeed = 55;
uint8_t activeProgram = 0, programStep = 0;
bool programWaiting = false;
uint32_t programResumeAt = 0, lastMotionAt = 0, lastBroadcastAt = 0, lastCommandAt = 0, lastJogAt = 0;
uint32_t sequenceNumber = 0, lastMotorSeenAt = 0, acknowledgedSequence = 0;

#if defined(DEVICE_ROLE_CONTROLLER)
uint8_t motorMac[6] = {};
bool motorPeerKnown = false;
volatile bool espNowRxPending = false;
uint8_t espNowRxMac[6] = {};
uint8_t espNowRxData[ESPNOW_MAX_PACKET_SIZE] = {};
volatile uint8_t espNowRxLength = 0;

void onControllerEspNowReceive(uint8_t *mac, uint8_t *data, uint8_t length) {
  if (!mac || !data || length > ESPNOW_MAX_PACKET_SIZE || espNowRxPending) return;
  memcpy(espNowRxMac, mac, 6);
  memcpy(espNowRxData, data, length);
  espNowRxLength = length;
  espNowRxPending = true;
}

void sendPairAcknowledgement(const uint8_t *targetMac) {
  EspNowPairPacket packet = {{ESPNOW_MAGIC, ESPNOW_PROTOCOL_VERSION, EspNowType::PairAck}, {}};
  memcpy(packet.targetMac, targetMac, 6);
  esp_now_send(const_cast<uint8_t *>(ESPNOW_BROADCAST_MAC), reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
}

void processControllerEspNow() {
  if (!espNowRxPending) return;
  uint8_t mac[6], data[ESPNOW_MAX_PACKET_SIZE], length;
  noInterrupts();
  memcpy(mac, espNowRxMac, 6); length = espNowRxLength;
  memcpy(data, espNowRxData, length); espNowRxPending = false;
  interrupts();
  if (length < sizeof(EspNowHeader)) return;
  const EspNowHeader &header = *reinterpret_cast<EspNowHeader *>(data);
  if (validEspNowHeader(header, EspNowType::Hello) && length == sizeof(EspNowHeader)) {
    if (!motorPeerKnown || memcmp(motorMac, mac, 6) != 0) {
      if (motorPeerKnown && esp_now_is_peer_exist(motorMac)) esp_now_del_peer(motorMac);
      memcpy(motorMac, mac, 6);
      motorPeerKnown = addEncryptedPeer(motorMac);
    }
    if (motorPeerKnown) sendPairAcknowledgement(motorMac);
    return;
  }
  if (validEspNowHeader(header, EspNowType::Status) && length == sizeof(EspNowStatusPacket) &&
      motorPeerKnown && memcmp(motorMac, mac, 6) == 0) {
    const EspNowStatusPacket &status = *reinterpret_cast<EspNowStatusPacket *>(data);
    acknowledgedSequence = status.acknowledgedSequence;
    lastMotorSeenAt = millis();
  }
}

void sendMotorCommand() {
  if (!motorPeerKnown) return;
  const int16_t pwm = map(driveSpeed, 0, 100, 0, PWM_MAX);
  const int16_t signedPwm = driveDirection == "right" ? pwm : driveDirection == "left" ? -pwm : 0;
  EspNowCommandPacket packet = {
    {ESPNOW_MAGIC, ESPNOW_PROTOCOL_VERSION, EspNowType::Command}, sequenceNumber, signedPwm
  };
  esp_now_send(motorMac, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
  lastCommandAt = millis();
}
#endif

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
  return lastMotorSeenAt && millis() - lastMotorSeenAt < MOTOR_LINK_TIMEOUT_MS;
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
#if defined(DEVICE_ROLE_SINGLE)
  d["transport"] = "local";
#else
  d["transport"] = "esp-now";
#endif
  d["ackSeq"] = acknowledgedSequence;
  if (lastMotorSeenAt) d["linkAgeMs"] = millis() - lastMotorSeenAt;
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
  if (!motorOnline() && mode != Mode::Idle) {
    stopAll();
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
    if (!strcmp(r, "ui") && client < MAX_CLIENTS) roles[client] = Role::Ui;
    String message = stateJson();
    wsServer.sendTXT(client, message); broadcastState(); return;
  }
  const Role role = client < MAX_CLIENTS ? roles[client] : Role::Unknown;
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
  WiFi.persistent(false);
#if defined(DEVICE_ROLE_CONTROLLER)
  WiFi.mode(WIFI_AP_STA); WiFi.disconnect();
#else
  WiFi.mode(WIFI_AP);
#endif
  WiFi.setOutputPower(20.5f); WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, false, 4);
#if defined(DEVICE_ROLE_CONTROLLER)
  if (!initEspNow(onControllerEspNowReceive)) Serial.println("ESP-NOW init failed");
#endif
  dnsServer.start(53, "*", IPAddress(192,168,4,1)); MDNS.begin("motor");
  httpServer.on("/", [](){ httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.onNotFound([](){ httpServer.sendHeader("Location", "/"); httpServer.send(302, "text/plain", ""); });
  httpServer.begin(); wsServer.begin(); wsServer.onEvent(handleWs);
  Serial.println("Open http://motor.local or http://192.168.4.1");
}

void loop() {
  dnsServer.processNextRequest(); httpServer.handleClient(); wsServer.loop(); MDNS.update();
#if defined(DEVICE_ROLE_CONTROLLER)
  processControllerEspNow();
#endif
  updateMotion();
#if defined(DEVICE_ROLE_SINGLE)
  updateLocalRamp();
#endif
  const uint32_t now = millis();
  if (now - lastBroadcastAt >= 100) broadcastState();
#if defined(DEVICE_ROLE_CONTROLLER)
  if (now - lastCommandAt >= ESPNOW_COMMAND_INTERVAL_MS) sendMotorCommand();
#endif
}

#elif defined(DEVICE_ROLE_MOTOR)

int16_t targetPwm = 0, appliedPwm = 0;
uint32_t lastPacketAt = 0, lastRampAt = 0, lastStatusAt = 0, lastPairAt = 0, receivedSequence = 0;
uint8_t controllerMac[6] = {};
bool controllerPaired = false, failsafeActive = true, haveCommandSequence = false;
volatile bool motorRxPending = false;
uint8_t motorRxMac[6] = {}, motorRxData[ESPNOW_MAX_PACKET_SIZE] = {};
volatile uint8_t motorRxLength = 0;
void writeMotor(int16_t pwm) {
  analogWrite(MOTOR_IN1_PIN, pwm > 0 ? pwm : 0);
  analogWrite(MOTOR_IN2_PIN, pwm < 0 ? -pwm : 0);
}
void emergencyStop() { targetPwm = appliedPwm = 0; failsafeActive = true; writeMotor(0); }
void onMotorEspNowReceive(uint8_t *mac, uint8_t *data, uint8_t length) {
  if (!mac || !data || length > ESPNOW_MAX_PACKET_SIZE || motorRxPending) return;
  memcpy(motorRxMac, mac, 6); memcpy(motorRxData, data, length);
  motorRxLength = length; motorRxPending = true;
}
void processMotorEspNow() {
  if (!motorRxPending) return;
  uint8_t mac[6], data[ESPNOW_MAX_PACKET_SIZE], length;
  noInterrupts();
  memcpy(mac, motorRxMac, 6); length = motorRxLength;
  memcpy(data, motorRxData, length); motorRxPending = false;
  interrupts();
  if (length < sizeof(EspNowHeader)) return;
  const EspNowHeader &header = *reinterpret_cast<EspNowHeader *>(data);
  if (validEspNowHeader(header, EspNowType::PairAck) && length == sizeof(EspNowPairPacket)) {
    uint8_t ownMac[6]; wifi_get_macaddr(STATION_IF, ownMac);
    const EspNowPairPacket &pairing = *reinterpret_cast<EspNowPairPacket *>(data);
    if (memcmp(pairing.targetMac, ownMac, 6) != 0) return;
    memcpy(controllerMac, mac, 6);
    controllerPaired = addEncryptedPeer(controllerMac);
    if (controllerPaired) { lastPacketAt = millis(); haveCommandSequence = false; }
    return;
  }
  if (validEspNowHeader(header, EspNowType::Command) && length == sizeof(EspNowCommandPacket) &&
      controllerPaired && memcmp(controllerMac, mac, 6) == 0) {
    const EspNowCommandPacket &command = *reinterpret_cast<EspNowCommandPacket *>(data);
    if (haveCommandSequence && int32_t(command.sequence - receivedSequence) < 0) return;
    targetPwm = constrain(command.pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
    receivedSequence = command.sequence;
    haveCommandSequence = true; lastPacketAt = millis(); failsafeActive = false;
  }
}
void ramp() {
  if (millis() - lastRampAt < RAMP_INTERVAL_MS) return;
  lastRampAt = millis();
  if (appliedPwm < targetPwm) { appliedPwm += RAMP_STEP; if (appliedPwm > targetPwm) appliedPwm = targetPwm; }
  else if (appliedPwm > targetPwm) { appliedPwm -= RAMP_STEP; if (appliedPwm < targetPwm) appliedPwm = targetPwm; }
  writeMotor(appliedPwm);
}
void sendStatus() {
  if (!controllerPaired) return;
  EspNowStatusPacket packet = {
    {ESPNOW_MAGIC, ESPNOW_PROTOCOL_VERSION, EspNowType::Status},
    receivedSequence, appliedPwm, uint8_t(failsafeActive)
  };
  esp_now_send(controllerMac, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
}
void sendPairRequest() {
  EspNowHeader hello = {ESPNOW_MAGIC, ESPNOW_PROTOCOL_VERSION, EspNowType::Hello};
  esp_now_send(const_cast<uint8_t *>(ESPNOW_BROADCAST_MAC), reinterpret_cast<uint8_t *>(&hello), sizeof(hello));
  lastPairAt = millis();
}
void setup() {
  Serial.begin(115200); pinMode(MOTOR_IN1_PIN, OUTPUT); pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteRange(PWM_MAX); analogWriteFreq(PWM_FREQUENCY_HZ); emergencyStop();
  WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.disconnect();
  WiFi.setOutputPower(20.5f); WiFi.setSleepMode(WIFI_NONE_SLEEP); wifi_set_channel(WIFI_CHANNEL);
  if (!initEspNow(onMotorEspNowReceive)) Serial.println("ESP-NOW init failed");
}
void loop() {
  processMotorEspNow();
  if (controllerPaired && millis() - lastPacketAt > MOTOR_PACKET_TIMEOUT_MS) {
    emergencyStop(); controllerPaired = false;
  }
  if (!controllerPaired && millis() - lastPairAt >= ESPNOW_PAIR_INTERVAL_MS) sendPairRequest();
  ramp();
  if (controllerPaired && millis() - lastStatusAt >= MOTOR_STATUS_INTERVAL_MS) {
    lastStatusAt = millis(); sendStatus();
  }
}

#else
#error Select controller, motor, or single environment
#endif
