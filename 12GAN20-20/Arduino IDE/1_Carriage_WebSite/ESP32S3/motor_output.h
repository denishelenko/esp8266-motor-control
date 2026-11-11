#pragma once
#include <Arduino.h>
#include "config.h"
#include "platform_hal.h"

// Both bridges receive the same two signals. Zero drives both inputs LOW.
class MotorOutput {
 public:
  bool begin() {
    digitalWrite(MOTOR_IN1_PIN, LOW); pinMode(MOTOR_IN1_PIN, OUTPUT);
    digitalWrite(MOTOR_IN2_PIN, LOW); pinMode(MOTOR_IN2_PIN, OUTPUT);
    ready_ = motorPwmBegin();
    write(0);
    return ready_;
  }

  void stop() {
    target_ = 0;
    if (applied_) { applied_ = 0; write(0); }
  }
  void setTarget(int16_t value) {
    if (!ready_) return;
    target_ = constrain(value, -int16_t(PWM_MAX), int16_t(PWM_MAX));
    if (!target_) stop();
  }
  int16_t applied() const { return applied_; }
  void tick(uint32_t now) {
    if (now - lastTick_ < RAMP_INTERVAL_MS) return;
    lastTick_ = now;
    const int8_t direction = target_ > 0 ? 1 : target_ < 0 ? -1 : 0;
    if (direction && lastDirection_ && direction != lastDirection_) {
      if (applied_) {
        applied_ = 0;
        write(0);
        return;
      }
      if (now - stoppedAt_ < REVERSAL_PAUSE_MS) return;
    }
    int16_t next = applied_;
    if (next < target_) next = min(int(target_), int(next) + RAMP_STEP);
    if (next > target_) next = max(int(target_), int(next) - RAMP_STEP);
    if (next != applied_) { applied_ = next; write(next); }
  }

 private:
  bool ready_ = false;
  int16_t target_ = 0, applied_ = 0, written_ = 0;
  int8_t lastDirection_ = 0;
  uint32_t lastTick_ = 0, stoppedAt_ = 0;
  void write(int16_t pwm) {
    if (!ready_) return;
    const int8_t direction = pwm > 0 ? 1 : pwm < 0 ? -1 : 0;
    const int16_t physicalPwm = MOTOR_DIRECTION_REVERSED ? -pwm : pwm;
    if (physicalPwm > 0) {
      ledcWrite(MOTOR_IN2_PIN, 0);
      ledcWrite(MOTOR_IN1_PIN, physicalPwm);
    } else if (physicalPwm < 0) {
      ledcWrite(MOTOR_IN1_PIN, 0);
      ledcWrite(MOTOR_IN2_PIN, -physicalPwm);
    } else {
      ledcWrite(MOTOR_IN1_PIN, 0);
      ledcWrite(MOTOR_IN2_PIN, 0);
    }
    if (pwm) {
      lastDirection_ = direction;
    } else if (written_) {
      stoppedAt_ = millis();
    }
    written_ = pwm;
  }
};
