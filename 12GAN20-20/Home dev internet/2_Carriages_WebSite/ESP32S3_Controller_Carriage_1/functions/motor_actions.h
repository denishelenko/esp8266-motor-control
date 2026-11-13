#pragma once

#include <math.h>

// These actions change one carriage only. The caller decides how to combine
// motor, servo, radio and other devices in a program.
bool moveMotorTo(CarriageState &c, float meters, uint8_t speedPercent) {
  if (!isfinite(meters) || meters < -TRACK_LIMIT_M || meters > TRACK_LIMIT_M ||
      speedPercent < 1 || speedPercent > 100) return false;
  c.target = meters;
  c.speed = speedPercent;
  c.mode = MotionMode::Target;
  return true;
}

bool moveMotorFor(CarriageState &c, int8_t direction, uint8_t speedPercent,
                  uint32_t durationMs, uint32_t now) {
  if ((direction != -1 && direction != 1) || speedPercent < 1 ||
      speedPercent > 100 || durationMs == 0 || durationMs > 120000) return false;
  c.target = direction > 0 ? TRACK_LIMIT_M : -TRACK_LIMIT_M;
  c.speed = speedPercent;
  c.timedStartedAt = now;
  c.timedDurationMs = durationMs;
  c.mode = MotionMode::Timed;
  return true;
}

void stopMotor(CarriageState &c) {
  c.target = c.position;
  c.outputPwm = 0;
  c.timedDurationMs = 0;
  c.mode = MotionMode::Idle;
}
