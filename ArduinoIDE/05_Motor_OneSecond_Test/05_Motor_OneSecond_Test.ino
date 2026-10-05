#include <Arduino.h>

// NodeMCU ESP8266 -> L298N channel A
// Remove the ENA jumper before connecting D5.
// D5 / GPIO14 -> ENA (PWM / motor power)
// D1 / GPIO5  -> IN1 (direction)
// D2 / GPIO4  -> IN2 (direction)
constexpr uint8_t MOTOR_ENA_PIN = 14;
constexpr uint8_t MOTOR_IN1_PIN = 5;
constexpr uint8_t MOTOR_IN2_PIN = 4;

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
  Serial.println("Motor test starts in 3 seconds");
  delay(START_DELAY_MS);
  Serial.println("Infinite left/right test started");
}

void loop() {
  const uint16_t testPwm = (PWM_MAX * TEST_POWER_PERCENT) / 100;
  uint32_t cycleNumber = 1;

  while (true) {
    Serial.print("Cycle ");
    Serial.println(cycleNumber++);

    Serial.println("LEFT: 2 seconds at 20%");
    runLeft(testPwm);
    delay(RUN_TIME_MS);

    Serial.println("STOP between directions: 1 second");
    stopMotor();
    delay(PAUSE_MS);

    Serial.println("RIGHT: 2 seconds at 20%");
    runRight(testPwm);
    delay(RUN_TIME_MS);

    Serial.println("STOP between directions: 1 second");
    stopMotor();
    delay(PAUSE_MS);
  }
}
