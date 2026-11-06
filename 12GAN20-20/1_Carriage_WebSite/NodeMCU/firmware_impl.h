#include <Arduino.h>
#include "platform_hal.h"
#include "config.h"
#include "motor_output.h"

#include <ArduinoJson.h>
#include <WebSocketsServer.h>

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

#include "web_ui.h"

LocalWebServer httpServer(HTTP_PORT);
WebSocketsServer wsServer(WEBSOCKET_PORT);
LocalDnsServer dnsServer;

enum class Role : uint8_t { Unknown, Ui };
enum class Mode : uint8_t { Idle, Target, Jog, Program, Calibration };
constexpr uint8_t MAX_CLIENTS = 8;
Role roles[MAX_CLIENTS] = {};
Mode mode = Mode::Idle;

float positionM = 0.0f, targetM = 0.0f;
String driveDirection = "stop";
uint8_t driveSpeed = 0, selectedSpeed = SPEED_DEFAULT_PERCENT;
uint8_t activeProgram = 0;
uint32_t programStep = 0;
bool programWaiting = false;
uint32_t programResumeAt = 0, lastMotionAt = 0, lastBroadcastAt = 0, lastJogAt = 0;
uint32_t calibrationStopAt = 0;
String calibrationDirection = "right";
bool homeWifiReported = false;
uint32_t sequenceNumber = 0;
uint32_t lastSerialHeartbeatAt = 0;
bool redOutputOn = false;
bool lightStrobeActive = false;
uint32_t lastLightToggleAt = 0;
bool motorHardwareReady = false;
bool networkReady = false;
int16_t motionOwner = -1;
uint32_t ownerSeenAt = 0;
void stopAll();

constexpr float limitM() { return TRACK_LENGTH_M * 0.5f - SAFE_MARGIN_M; }
const char *modeName() {
  switch (mode) {
    case Mode::Target: return "рух до цілі";
    case Mode::Jog: return "ручний рух";
    case Mode::Program: return "програма";
    case Mode::Calibration: return "калібрування";
    default: return "idle";
  }
}

MotorOutput localMotor;

void writeLights(bool redEnabled) {
  if (redOutputOn != redEnabled) {
    redOutputOn = redEnabled;
    digitalWrite(LIGHT_RED_PIN, redEnabled ? HIGH : LOW);
  }
}

void updateLight(uint32_t now) {
  const bool motorIsMoving = localMotor.applied() != 0;
  if (!motorIsMoving) {
    lightStrobeActive = false;
    writeLights(false);
    digitalWrite(LIGHT_GREEN_PIN, HIGH);
    return;
  }

  // While moving, green stays off and only the red channel strobes.
  digitalWrite(LIGHT_GREEN_PIN, LOW);

  if (!lightStrobeActive) {
    lightStrobeActive = true;
    lastLightToggleAt = now;
    writeLights(true);
    return;
  }

  if (now - lastLightToggleAt >= LIGHT_STROBE_HALF_PERIOD_MS) {
    lastLightToggleAt = now;
    writeLights(!redOutputOn);
  }
}

uint8_t servoOpenAngle = SERVO_DEFAULT_OPEN_ANGLE;
uint8_t servoCurrentAngle = SERVO_CLOSED_ANGLE;
bool servoCycleEnabled = false, servoIsOpen = false;
enum class ServoPhase : uint8_t { ClosedPause, Opening, OpenPause, Closing };
ServoPhase servoPhase = ServoPhase::ClosedPause;
uint32_t servoNextActionAt = 0;

void writeServoPosition(uint8_t physicalAngle) {
  physicalAngle = constrain(physicalAngle, SERVO_CLOSED_ANGLE, SERVO_PHYSICAL_MAX_ANGLE);
  servoCurrentAngle = physicalAngle;
  const uint8_t commandAngle = physicalAngle * SERVO_COMMAND_PER_PHYSICAL_DEGREE;
  if (!servoWriteAngle(commandAngle)) {
    servoCycleEnabled = false;
    servoDetach();
    digitalWrite(SERVO_SIGNAL_PIN, LOW);
    Serial.println("[ERROR] Servo waveform failed");
  }
}

void stopServoCycle() {
  const bool wasRunning = servoCycleEnabled || servoIsOpen;
  if (!wasRunning) return;
  servoCycleEnabled = false;
  servoIsOpen = false;
  servoPhase = ServoPhase::ClosedPause;
  writeServoPosition(SERVO_CLOSED_ANGLE);
  Serial.println("[SERVO] цикл зупинено");
}

void startServoCycle() {
  servoCycleEnabled = true;
  servoIsOpen = false;
  servoPhase = ServoPhase::ClosedPause;
  writeServoPosition(SERVO_CLOSED_ANGLE);
  servoNextActionAt = millis() + SERVO_CLOSED_HOLD_MS;
  Serial.print("[SERVO] цикл запущено, відкриття до ");
  Serial.print(servoOpenAngle);
  Serial.println("°");
}

void updateServoCycle() {
  if (!servoCycleEnabled || mode != Mode::Program) return;
  const uint32_t now = millis();
  if (int32_t(now - servoNextActionAt) < 0) return;

  switch (servoPhase) {
    case ServoPhase::ClosedPause:
      servoPhase = ServoPhase::Opening;
      servoNextActionAt = now;
      break;

    case ServoPhase::Opening: {
      // Do not slow the mechanism down in software: jump immediately to target.
      writeServoPosition(servoOpenAngle);
      servoIsOpen = true;
      servoPhase = ServoPhase::OpenPause;
      servoNextActionAt = now + SERVO_OPEN_HOLD_MS;
      break;
    }

    case ServoPhase::OpenPause:
      servoIsOpen = false;
      servoPhase = ServoPhase::Closing;
      servoNextActionAt = now;
      break;

    case ServoPhase::Closing: {
      // Same immediate movement back to the physical closed position.
      writeServoPosition(SERVO_CLOSED_ANGLE);
      servoPhase = ServoPhase::ClosedPause;
      servoNextActionAt = now + SERVO_CLOSED_HOLD_MS;
      break;
    }
  }
}

const char *servoPhaseName() {
  switch (servoPhase) {
    case ServoPhase::Opening: return "opening";
    case ServoPhase::OpenPause: return "open";
    case ServoPhase::Closing: return "closing";
    default: return "closed";
  }
}

bool motorOnline() {
  return networkReady && motorHardwareReady;
}

void setDrive(const char *direction, uint8_t speed) {
  speed = constrain(speed, 0, 100);
  if (strcmp(direction, "stop") == 0) speed = 0;
  if (driveDirection == direction && driveSpeed == speed) return;
  driveDirection = direction;
  driveSpeed = speed;
  ++sequenceNumber;
  Serial.print("[MOTOR] ");
  Serial.print(driveDirection == "right" ? "ВПРАВО" : driveDirection == "left" ? "ВЛІВО" : "СТОП");
  Serial.print(" | швидкість: ");
  Serial.print(driveSpeed);
  Serial.println('%');
  const int16_t pwm = map(speed, 0, 100, 0, PWM_MAX);
  localMotor.setTarget(driveDirection == "right" ? pwm : driveDirection == "left" ? -pwm : 0);
}

String stateJson() {
  JsonDocument d;
  d["lightOn"] = redOutputOn;
  d["lightStrobe"] = lightStrobeActive;
  d["lightOutput"] = redOutputOn;
  d["lightColor"] = redOutputOn ? "red" : "off";
  d["type"] = "state"; d["seq"] = sequenceNumber;
  d["position"] = positionM; d["target"] = targetM; d["limit"] = limitM();
  d["direction"] = driveDirection; d["speed"] = selectedSpeed; d["driveSpeed"] = driveSpeed;
  d["mode"] = modeName(); d["program"] = activeProgram; d["motorOnline"] = motorOnline();
  d["calibrationDirection"] = calibrationDirection;
  d["calibrationSecondsRemaining"] = mode == Mode::Calibration && calibrationStopAt > millis()
    ? (calibrationStopAt - millis()) / 1000.0f : 0.0f;
  d["transport"] = "local";
  d["appliedPwm"] = localMotor.applied();
  d["servoAvailable"] = true;
  d["servoAngle"] = servoOpenAngle;
  d["servoCurrentAngle"] = servoCurrentAngle;
  d["servoEnabled"] = servoCycleEnabled;
  d["servoOpen"] = servoIsOpen;
  d["servoPhase"] = servoPhaseName();
  String out; serializeJson(d, out); return out;
}

void broadcastState() {
  lastBroadcastAt = millis();
  bool anyUi = false;
  for (uint8_t i = 0; i < MAX_CLIENTS; ++i) anyUi |= roles[i] == Role::Ui;
  if (!anyUi) return;
  String message = stateJson();
  wsServer.broadcastTXT(message);
}
void stopAll() {
  motionOwner = -1;
  mode = Mode::Idle; activeProgram = 0; programWaiting = false; calibrationStopAt = 0; targetM = positionM;
  setDrive("stop", 0);
  stopServoCycle();
  broadcastState();
}

void chooseProgramTarget() {
  static const float p1[] = {-1.70f, 1.70f};
  static const float p2[] = {-0.45f, 0.45f, -0.90f, 0.90f, -1.35f, 1.35f, -1.70f, 1.70f};
  if (activeProgram == 1) targetM = p1[programStep % 2];
  else if (activeProgram == 2) targetM = p2[programStep % 8];
  else if (activeProgram == 3) {
    // Ривки: короткі непередбачувані переміщення на 1–3 м із повною
    // потужністю. Біля краю напрямок автоматично змінюється.
    const float jump = 1.0f + float(random(0, 21)) / 10.0f;
    int direction = random(0, 2) ? 1 : -1;
    float candidate = positionM + direction * jump;
    if (candidate > limitM() || candidate < -limitM()) {
      direction = -direction;
      candidate = positionM + direction * jump;
    }
    targetM = constrain(candidate, -limitM(), limitM());
  } else {
    // Рандом: довгі переходи щонайменше приблизно на 4 м від поточної точки.
    const float distance = 4.0f + float(random(0, 31)) / 10.0f;
    int direction = random(0, 2) ? 1 : -1;
    float candidate = positionM + direction * distance;
    if (candidate > limitM() || candidate < -limitM()) {
      direction = -direction;
      candidate = positionM + direction * distance;
    }
    targetM = constrain(candidate, -limitM(), limitM());
  }
  programWaiting = false;
}

void reachedTarget() {
  positionM = targetM; setDrive("stop", 0);
  if (mode == Mode::Program) {
    programWaiting = true; ++programStep;
    programResumeAt = millis() + 300;
  } else mode = Mode::Idle;
  broadcastState();
}

void updateMotion() {
  const uint32_t now = millis();
  if (!lastMotionAt) { lastMotionAt = now; return; }
  if (mode != Mode::Idle && now - lastMotionAt > 250) {
    lastMotionAt = now;
    Serial.println("[SAFE] Main loop delayed; motion stopped");
    stopAll(); return;
  }
  float dt = (now - lastMotionAt) / 1000.0f; lastMotionAt = now;
  if (dt > 0.10f) dt = 0.10f;
  if (mode != Mode::Idle && (motionOwner < 0 || now - ownerSeenAt > UI_COMMAND_LEASE_MS || !motorOnline())) {
    stopAll(); return;
  }
  if (mode == Mode::Jog && now - lastJogAt > JOG_DEADMAN_TIMEOUT_MS) { stopAll(); return; }
  if (mode == Mode::Program && programWaiting) {
    if ((int32_t)(now - programResumeAt) >= 0) chooseProgramTarget(); else return;
  }
  if (mode == Mode::Calibration) {
    if ((int32_t)(now - calibrationStopAt) >= 0) {
      stopAll();
      return;
    }
    setDrive(calibrationDirection.c_str(), selectedSpeed);
    return;
  }
  if (!motorOnline() && mode != Mode::Idle) {
    stopAll();
    return;
  }
  if (mode == Mode::Target || mode == Mode::Program) {
    const float error = targetM - positionM;
    if (fabs(error) <= POSITION_TOLERANCE_M) { reachedTarget(); return; }
    const bool right = error > 0;
    const int16_t applied = localMotor.applied();
    const uint8_t appliedPercent = (uint32_t(abs(applied)) * 100U + PWM_MAX / 2U) / PWM_MAX;
    const float step = calibratedSpeed(appliedPercent, applied >= 0) * dt;
    const uint8_t motionSpeed = activeProgram == 3 ? SPEED_MAX_PERCENT : selectedSpeed;
    setDrive(right ? "right" : "left", motionSpeed);
    if ((applied > 0) == right && step >= fabs(error)) { reachedTarget(); return; }
    positionM += applied > 0 ? step : applied < 0 ? -step : 0;
    positionM = constrain(positionM, -limitM(), limitM());
  }
  // Manual left/right movement uses the user's PWM percentage directly.
  // Without an encoder it does not change the estimated coordinate here.
  // The dead-man timeout above still stops the motor if browser commands stop.
}

void handleWs(uint8_t client, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_DISCONNECTED) {
    if (client < MAX_CLIENTS) roles[client] = Role::Unknown;
    if (motionOwner == client) stopAll();
    return;
  }
  if (type == WStype_CONNECTED) {
    if (client < MAX_CLIENTS) roles[client] = Role::Unknown;
    String message = stateJson();
    wsServer.sendTXT(client, message); return;
  }
  if (type != WStype_TEXT || length == 0 || length > 256 || client >= MAX_CLIENTS) return;
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
  if (!strcmp(t, "release")) { if (motionOwner == client) stopAll(); return; }
  if (!strcmp(t, "ping")) {
    if (motionOwner == client) ownerSeenAt = millis();
    return;
  }
  if (!d["speed"].isNull() && (!d["speed"].is<int>() ||
      d["speed"].as<int>() < SPEED_MIN_PERCENT || d["speed"].as<int>() > SPEED_MAX_PERCENT)) {
    // A stop command never depends on a speed field.
    if (strcmp(t, "stop") && !(strcmp(t, "jog") == 0 && strcmp(d["direction"] | "", "stop") == 0)) return;
  }
  if (!strcmp(t, "stop")) { stopAll(); return; }
  if (!strcmp(t, "zero")) { stopAll(); positionM = targetM = 0; broadcastState(); return; }
  if (!strcmp(t, "target")) {
    if (!motorOnline() || !d["position"].is<float>() || !isfinite(d["position"].as<float>())) return;
    motionOwner = client; ownerSeenAt = millis();
    stopServoCycle();
    activeProgram = 0; programWaiting = false; mode = Mode::Target;
    targetM = constrain(d["position"] | 0.0f, -limitM(), limitM());
    selectedSpeed = constrain(d["speed"] | selectedSpeed, SPEED_MIN_PERCENT, SPEED_MAX_PERCENT); broadcastState(); return;
  }
  if (!strcmp(t, "program")) {
    if (!motorOnline() || !d["id"].is<int>() || d["id"].as<int>() < 1 || d["id"].as<int>() > 4) return;
    motionOwner = client; ownerSeenAt = millis();
    const uint8_t id = d["id"].as<uint8_t>();
    selectedSpeed = constrain(d["speed"] | selectedSpeed, SPEED_MIN_PERCENT, SPEED_MAX_PERCENT);
    mode = Mode::Program; activeProgram = id; programStep = 0;
    startServoCycle();
    chooseProgramTarget(); broadcastState(); return;
  }
  if (!strcmp(t, "calibration")) {
    if (!motorOnline() || !d["seconds"].is<int>() || d["seconds"].as<int>() < 1 ||
        d["seconds"].as<int>() > 4) return;
    const char *dir = d["direction"] | "right";
    if (strcmp(dir, "right") && strcmp(dir, "left")) return;
    motionOwner = client; ownerSeenAt = millis();
    stopServoCycle();
    activeProgram = 0; programWaiting = false; mode = Mode::Calibration;
    calibrationDirection = dir;
    selectedSpeed = constrain(d["speed"] | selectedSpeed, SPEED_MIN_PERCENT, SPEED_MAX_PERCENT);
    calibrationStopAt = millis() + uint32_t(d["seconds"].as<int>()) * 1000UL;
    setDrive(calibrationDirection.c_str(), selectedSpeed);
    broadcastState(); return;
  }
  if (!strcmp(t, "servo")) {
    const bool enabled = d["enabled"] | true;
    if (!enabled) {
      stopServoCycle();
    } else {
      const uint8_t requestedAngle = constrain(d["angle"] | SERVO_DEFAULT_OPEN_ANGLE,
                                                SERVO_CLOSED_ANGLE, SERVO_PHYSICAL_MAX_ANGLE);
      servoOpenAngle = requestedAngle >= 68 ? 90 : 45;
      if (mode == Mode::Program) {
        if (!servoCycleEnabled) startServoCycle();
        else {
          servoIsOpen = false;
          servoPhase = servoCurrentAngle > servoOpenAngle
            ? ServoPhase::Closing
            : ServoPhase::Opening;
          servoNextActionAt = millis();
        }
      }
    }
    broadcastState();
    return;
  }
  if (!strcmp(t, "speed")) {
    selectedSpeed = constrain(d["speed"] | selectedSpeed, SPEED_MIN_PERCENT, SPEED_MAX_PERCENT);
    if (mode == Mode::Jog) setDrive(driveDirection.c_str(), selectedSpeed);
    broadcastState(); return;
  }
  if (!strcmp(t, "jog")) {
    const char *dir = d["direction"] | "stop";
    if (!strcmp(dir, "stop")) {
      if (mode == Mode::Jog && motionOwner == client) stopAll();
      return;
    }
    if (!motorOnline() || (strcmp(dir, "right") && strcmp(dir, "left"))) return;
    motionOwner = client; ownerSeenAt = millis();
    stopServoCycle();
    mode = Mode::Jog; activeProgram = 0; lastJogAt = millis();
    selectedSpeed = constrain(d["speed"] | selectedSpeed, SPEED_MIN_PERCENT, SPEED_MAX_PERCENT);
    setDrive(!strcmp(dir, "right") ? "right" : "left", selectedSpeed); return;
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD); randomSeed(micros());
  pinMode(LIGHT_RED_PIN, OUTPUT); pinMode(LIGHT_GREEN_PIN, OUTPUT);
  digitalWrite(LIGHT_RED_PIN, LOW); digitalWrite(LIGHT_GREEN_PIN, HIGH);
  printBootInfo();
  Serial.println("[START] Standalone: motor + servo + RGB status (green idle / red strobe while moving)");
  motorHardwareReady = localMotor.begin();
  if (!motorHardwareReady) Serial.println("[ERROR] Motor PWM init failed; motion disabled");
  digitalWrite(SERVO_SIGNAL_PIN, LOW); pinMode(SERVO_SIGNAL_PIN, OUTPUT);
  // No servo pulse/movement until the user starts a program.
  Serial.println(PINOUT_SERIAL_TEXT);
  WiFi.persistent(false);
  #if HOME_WIFI_CONFIGURED
  WiFi.mode(WIFI_AP_STA);
  #else
  WiFi.mode(WIFI_AP);
  #endif
  disableWifiSleep();
  Serial.println("[WIFI] Starting Motor-Control AP...");
  networkReady = WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0))
    && WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, false, 4);
  Serial.printf("[WIFI] %s IP=%s channel=%ld\n", networkReady ? "OK" : "FAILED",
    WiFi.softAPIP().toString().c_str(), static_cast<long>(WiFi.channel()));
  #if HOME_WIFI_CONFIGURED
  WiFi.setAutoReconnect(true);
  WiFi.begin(HOME_WIFI_SSID, HOME_WIFI_PASSWORD);
  Serial.printf("[WIFI] Joining home network '%s'...\n", HOME_WIFI_SSID);
  #endif
  dnsServer.start(53, "*", IPAddress(192,168,4,1));
  MDNS.begin(
    #if HOME_WIFI_CONFIGURED
    HOME_MDNS_HOSTNAME
    #else
    "motor"
    #endif
  );
  httpServer.on("/", [](){ httpServer.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  httpServer.onNotFound([](){ httpServer.sendHeader("Location", "/"); httpServer.send(302, "text/plain", ""); });
  httpServer.begin(); wsServer.begin(); wsServer.onEvent(handleWs);
  Serial.println("Open http://motor.local or http://192.168.4.1");
}

void loop() {
  dnsServer.processNextRequest(); httpServer.handleClient(); wsServer.loop(); updateMdns();
  updateMotion();
  const uint32_t now = millis();
  localMotor.tick(now);
  updateLight(now);
  updateServoCycle();
  if (now - lastBroadcastAt >= UI_BROADCAST_INTERVAL_MS) broadcastState();
  #if HOME_WIFI_CONFIGURED
  if (WiFi.status() == WL_CONNECTED && !homeWifiReported) {
    Serial.printf("[WIFI] Home network connected, IP=%s, site=http://%s.local/\n",
      WiFi.localIP().toString().c_str(), HOME_MDNS_HOSTNAME);
    homeWifiReported = true;
  } else if (WiFi.status() != WL_CONNECTED) {
    homeWifiReported = false;
  }
  #endif
  if (now - lastSerialHeartbeatAt >= 3000) {
    lastSerialHeartbeatAt = now;
    Serial.print("[OK] ESP працює | режим: ");
    Serial.print(modeName());
    Serial.print(" | мотор: ");
    Serial.print(driveDirection == "right" ? "ВПРАВО" : driveDirection == "left" ? "ВЛІВО" : "СТОП");
    Serial.print(" | швидкість: ");
    Serial.print(driveSpeed);
    Serial.print('%');
    Serial.println();
  }
  delay(1); // Yield to the Wi-Fi stack and watchdog.
}
