#pragma once

// ESP32-S3 motor, servo and indicator outputs.

void writeLocalMotor(int16_t pwm) {
  pwm = constrain(pwm, -int16_t(PWM_MAX), int16_t(PWM_MAX));
  if (MOTOR_DIRECTION_REVERSED) pwm = -pwm;
  if (pwm > 0) {
    ledcWrite(MOTOR_IN2_PIN, 0);
    ledcWrite(MOTOR_IN1_PIN, pwm);
  } else if (pwm < 0) {
    ledcWrite(MOTOR_IN1_PIN, 0);
    ledcWrite(MOTOR_IN2_PIN, -pwm);
  } else {
    ledcWrite(MOTOR_IN1_PIN, 0);
    ledcWrite(MOTOR_IN2_PIN, 0);
  }
}

void writeLights(bool redEnabled) {
  if (redOutputOn != redEnabled) {
    redOutputOn = redEnabled;
    digitalWrite(LIGHT_RED_PIN, redEnabled ? HIGH : LOW);
  }
}

void updateLights(uint32_t now) {
  if (localAppliedPwm == 0) {
    lightStrobeActive = false;
    writeLights(false);
    digitalWrite(LIGHT_GREEN_PIN, HIGH);
    return;
  }
  digitalWrite(LIGHT_GREEN_PIN, LOW);
  if (!lightStrobeActive) {
    lightStrobeActive = true;
    lastLightToggleAt = now;
    writeLights(true);
    return;
  }
  if (now - lastLightToggleAt >= LIGHT_BLINK_HALF_PERIOD_MS) {
    lastLightToggleAt = now;
    writeLights(!redOutputOn);
  }
}

void updateLocalMotor() {
  const uint32_t now = millis();
  const int16_t targetPwm = carriages[0].outputPwm;
  if (targetPwm == 0 && localAppliedPwm != 0) {
    localAppliedPwm = 0;
    lastLocalRampAt = now;
    writeLocalMotor(0);
    return;
  }
  if (localAppliedPwm == targetPwm) return;
  if (now - lastLocalRampAt < RAMP_INTERVAL_MS) return;
  lastLocalRampAt = now;
  if ((localAppliedPwm > 0 && targetPwm < 0) || (localAppliedPwm < 0 && targetPwm > 0)) {
    localAppliedPwm = 0;
  } else if (localAppliedPwm < targetPwm) {
    localAppliedPwm += RAMP_STEP;
    if (localAppliedPwm > targetPwm) localAppliedPwm = targetPwm;
  } else if (localAppliedPwm > targetPwm) {
    localAppliedPwm -= RAMP_STEP;
    if (localAppliedPwm < targetPwm) localAppliedPwm = targetPwm;
  }
  writeLocalMotor(localAppliedPwm);
}

void writeLocalServo(uint8_t physicalAngle) {
  const uint8_t commandAngle = constrain(uint16_t(physicalAngle) * 2, uint16_t(0), uint16_t(180));
  const uint16_t pulseUs = SERVO_MIN_PULSE_US +
      (uint32_t(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * commandAngle) / 180;
  const uint32_t duty = (uint64_t(pulseUs) * SERVO_FREQUENCY_HZ * 16384ULL + 500000ULL) / 1000000ULL;
  ledcWrite(SERVO_SIGNAL_PIN, duty);
}
