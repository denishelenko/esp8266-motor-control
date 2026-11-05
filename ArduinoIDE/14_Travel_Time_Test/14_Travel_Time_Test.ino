#include <Arduino.h>

// One self-contained sketch for either carriage. In Arduino IDE select the
// actual board: NodeMCU v3 (ESP8266) or ESP32-S3 DevKitC-1.
#if defined(ARDUINO_ARCH_ESP8266)
constexpr uint8_t MOTOR_ENA_PIN = 5;   // D1 -> L298N ENA; remove ENA jumper.
constexpr uint8_t MOTOR_IN1_PIN = 4;   // D2 -> IN1.
constexpr uint8_t MOTOR_IN2_PIN = 14;  // D5 -> IN2.
#elif defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t MOTOR_ENA_PIN = 4;
constexpr uint8_t MOTOR_IN1_PIN = 5;
constexpr uint8_t MOTOR_IN2_PIN = 6;
#else
#error Select an ESP8266 NodeMCU or ESP32-S3 board.
#endif

constexpr uint16_t PWM_MAX = 1023;
constexpr float WORKING_TRAVEL_M = 16.0f;  // 20 m rope minus 2 m reserve on each end.
constexpr uint32_t MAX_RUN_MS = 120000;  // Safety timeout; does not replace a physical stop switch.
uint8_t speedPercent = 50;               // Matches TRAVEL_CALIBRATION_SPEED_PERCENT in controller config.h.
int8_t direction = 0;
uint32_t startedAt = 0;
char line[16] = {};
uint8_t lineLength = 0;

void writePwm(uint16_t duty) {
#if defined(ARDUINO_ARCH_ESP8266)
  analogWrite(MOTOR_ENA_PIN, duty);
#else
  ledcWrite(MOTOR_ENA_PIN, duty);
#endif
}

void stopRun(const char *reason) {
  writePwm(0);
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
  if (!direction) return;
  const float seconds = (millis() - startedAt) / 1000.0f;
  Serial.print(reason);
  Serial.print(F(" | direction: "));
  Serial.print(direction > 0 ? F("RIGHT") : F("LEFT"));
  Serial.print(F(" | elapsed time (valid for 16 m only if stopped at the mark): "));
  Serial.print(seconds, 3);
  Serial.print(F(" s | assumed 16/time: "));
  Serial.print(seconds > 0 ? WORKING_TRAVEL_M / seconds : 0, 3);
  Serial.println(F(" m/s"));
  direction = 0;
}

void startRun(int8_t nextDirection) {
  if (direction) {
    Serial.println(F("Stop the current run with S before reversing."));
    return;
  }
  direction = nextDirection;
  digitalWrite(MOTOR_IN1_PIN, nextDirection > 0 ? HIGH : LOW);
  digitalWrite(MOTOR_IN2_PIN, nextDirection > 0 ? LOW : HIGH);
  startedAt = millis();
  writePwm(uint32_t(PWM_MAX) * speedPercent / 100);
  Serial.print(F("Started "));
  Serial.print(nextDirection > 0 ? F("RIGHT") : F("LEFT"));
  Serial.print(F(" at "));
  Serial.print(speedPercent);
  Serial.println(F("%. Press S at the 16 m working mark."));
}

void processLine() {
  if (lineLength == 0) return;
  line[lineLength] = '\0';
  if (line[0] == 'S' || line[0] == 's') stopRun("Stopped");
  else if (line[0] == 'L' || line[0] == 'l') startRun(-1);
  else if (line[0] == 'R' || line[0] == 'r') startRun(1);
  else if (line[0] == 'P' || line[0] == 'p') {
    if (direction) Serial.println(F("Stop first, then change PWM percentage."));
    else {
      const int value = atoi(line + 1);
      if (value < 1 || value > 100) Serial.println(F("Use P1..P100."));
      else { speedPercent = value; Serial.print(F("Speed: ")); Serial.println(speedPercent); }
    }
  } else Serial.println(F("Commands: R, L, S, P1..P100."));
  lineLength = 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
#if defined(ARDUINO_ARCH_ESP8266)
  pinMode(MOTOR_ENA_PIN, OUTPUT);
  analogWriteRange(PWM_MAX);
  analogWriteFreq(1000);
#else
  ledcAttach(MOTOR_ENA_PIN, 1000, 10);
#endif
  writePwm(0);
  Serial.println(F("16 m travel-time test READY. Motor stays OFF until R or L."));
  Serial.println(F("Mark both working ends 2 m inside the physical rope ends."));
  Serial.println(F("Commands: R/right, L/left, S/stop at mark, P50/speed 50%."));
}

void loop() {
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch == '\r' || ch == '\n') processLine();
    else if (lineLength < sizeof(line) - 1) line[lineLength++] = ch;
  }
  if (direction && millis() - startedAt >= MAX_RUN_MS) stopRun("TIMEOUT");
  delay(1);
}
