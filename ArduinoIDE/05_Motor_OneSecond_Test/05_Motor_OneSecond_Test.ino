#include <Arduino.h>

// NodeMCU ESP8266 -> MX1508
// D1 / GPIO5 -> IN1
// D2 / GPIO4 -> IN2
constexpr uint8_t MOTOR_IN1_PIN = 5;
constexpr uint8_t MOTOR_IN2_PIN = 4;

constexpr uint16_t PWM_MAX = 1023;
constexpr uint8_t TEST_POWER_PERCENT = 20;
constexpr uint32_t START_DELAY_MS = 3000;
constexpr uint32_t RUN_TIME_MS = 2000;
constexpr uint32_t PAUSE_MS = 1000;

void stopMotor() {
  analogWrite(MOTOR_IN1_PIN, 0);
  analogWrite(MOTOR_IN2_PIN, 0);
}

void runLeft(uint16_t pwm) {
  analogWrite(MOTOR_IN1_PIN, 0);
  analogWrite(MOTOR_IN2_PIN, pwm);
}

void runRight(uint16_t pwm) {
  analogWrite(MOTOR_IN2_PIN, 0);
  analogWrite(MOTOR_IN1_PIN, pwm);
}

void setup() {
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
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
