#include <Arduino.h>

// Same signal pins and pulse settings as the two-carriage firmware.
#if defined(ARDUINO_ARCH_ESP8266)
#include <Servo.h>
constexpr uint8_t SERVO_PIN = 12;  // NodeMCU D6.
Servo testServo;
#elif defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t SERVO_PIN = 15;  // ESP32-S3 GPIO15.
constexpr uint16_t SERVO_MIN_PULSE_US = 520;
constexpr uint16_t SERVO_MAX_PULSE_US = 2520;
constexpr uint16_t SERVO_FREQUENCY_HZ = 333;
#else
#error Select an ESP8266 NodeMCU or ESP32-S3 board.
#endif

char line[16] = {};
uint8_t lineLength = 0;
uint8_t commandedAngle = 0;  // Raw Servo.write angle: 90 ~= physical 45, 180 ~= physical 90.
uint32_t commandedAt = 0;

void writeServo(uint8_t angle) {
#if defined(ARDUINO_ARCH_ESP8266)
  testServo.write(angle);
#else
  const uint16_t pulseUs = SERVO_MIN_PULSE_US +
      uint32_t(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle / 180;
  const uint32_t duty = (uint64_t(pulseUs) * SERVO_FREQUENCY_HZ * 16384ULL + 500000ULL) / 1000000ULL;
  ledcWrite(SERVO_PIN, duty);
#endif
}

void processLine() {
  if (lineLength == 0) return;
  line[lineLength] = '\0';
  if (line[0] == 'D' || line[0] == 'd') {
    if (!commandedAt) Serial.println(F("Send 0, 90 or 180 first."));
    else {
      Serial.print(F("Reached raw command "));
      Serial.print(commandedAngle);
      Serial.print(F(" after approx. "));
      Serial.print((millis() - commandedAt) / 1000.0f, 3);
      Serial.println(F(" s (includes your reaction time)."));
    }
  } else {
    const int value = atoi(line);
    if (strcmp(line, "0") && strcmp(line, "90") && strcmp(line, "180"))
      Serial.println(F("Use 0, 90, 180 or D."));
    else {
      commandedAngle = value;
      commandedAt = millis();
      writeServo(commandedAngle);
      Serial.print(F("Commanded raw angle "));
      Serial.print(commandedAngle);
      Serial.println(F(". Send D when physical motion finishes."));
    }
  }
  lineLength = 0;
}

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP8266)
  testServo.attach(SERVO_PIN);
#else
  ledcAttachChannel(SERVO_PIN, SERVO_FREQUENCY_HZ, 14, 2);
#endif
  writeServo(0);
  Serial.println(F("Servo-only timing test READY at 115200 baud."));
  Serial.println(F("Separate regulated 5-8.4 V servo supply and common GND required."));
  Serial.println(F("Commands: 0/closed, 90/about 45 physical, 180/about 90 physical, D/done."));
}

void loop() {
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch == '\r' || ch == '\n') processLine();
    else if (lineLength < sizeof(line) - 1) line[lineLength++] = ch;
  }
  delay(1);
}
