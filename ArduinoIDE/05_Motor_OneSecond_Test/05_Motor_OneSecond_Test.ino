#include <Arduino.h>

// NodeMCU ESP8266 -> L298N channel A
// Remove the ENA jumper before connecting D6.
// D6 / GPIO12 -> ENA (PWM / motor power)
// D7 / GPIO13 -> IN1 (direction)
// D8 / GPIO15 -> IN2 (direction; must stay LOW at boot)
constexpr uint8_t MOTOR_ENA_PIN = 12;
constexpr uint8_t MOTOR_IN1_PIN = 13;
constexpr uint8_t MOTOR_IN2_PIN = 15;

constexpr uint16_t PWM_MAX = 1023;
constexpr uint8_t TEST_POWER_PERCENT = 20;
constexpr uint32_t START_DELAY_MS = 3000;
constexpr uint32_t RUN_TIME_MS = 2000;
constexpr uint32_t PAUSE_MS = 1000;

void stopMotor() {
  analogWrite(MOTOR_ENA_PIN, 0);
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
}

void runLeft(uint16_t pwm) {
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, HIGH);
  analogWrite(MOTOR_ENA_PIN, pwm);
}

void runRight(uint16_t pwm) {
  digitalWrite(MOTOR_IN1_PIN, HIGH);
  digitalWrite(MOTOR_IN2_PIN, LOW);
  analogWrite(MOTOR_ENA_PIN, pwm);
}

void setup() {
  pinMode(MOTOR_ENA_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteRange(PWM_MAX);
  analogWriteFreq(1000);
  stopMotor();

  Serial.begin(115200);
  Serial.println();
  Serial.println("[START] ESP8266: безкінечний тест мотора");
  Serial.println("[PINS] ENA=D6, IN1=D7, IN2=D8");
  Serial.println("[OK] Тест почнеться через 3 секунди");
  delay(START_DELAY_MS);
  Serial.println("[OK] Безкінечний тест запущено");
}

void loop() {
  const uint16_t testPwm = (PWM_MAX * TEST_POWER_PERCENT) / 100;
  uint32_t cycleNumber = 1;

  while (true) {
    Serial.print("[OK] ESP працює | цикл ");
    Serial.println(cycleNumber++);

    Serial.print("[MOTOR] ВЛІВО | час: 2 с | швидкість: ");
    Serial.print(TEST_POWER_PERCENT);
    Serial.println('%');
    runLeft(testPwm);
    delay(RUN_TIME_MS);

    Serial.println("[MOTOR] СТОП | пауза 1 с");
    stopMotor();
    delay(PAUSE_MS);

    Serial.print("[MOTOR] ВПРАВО | час: 2 с | швидкість: ");
    Serial.print(TEST_POWER_PERCENT);
    Serial.println('%');
    runRight(testPwm);
    delay(RUN_TIME_MS);

    Serial.println("[MOTOR] СТОП | пауза 1 с");
    stopMotor();
    delay(PAUSE_MS);
  }
}
