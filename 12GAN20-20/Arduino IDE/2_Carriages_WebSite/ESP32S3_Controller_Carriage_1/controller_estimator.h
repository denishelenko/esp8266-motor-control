#pragma once

// Measured 16 m travel times live in config.h. Zero uses a placeholder speed.
float estimatedSpeedMps(uint8_t index, bool right, uint8_t speedPercent) {
  const float measuredTime = right ? TRAVEL_TIMES[index].right16mSeconds
                                   : TRAVEL_TIMES[index].left16mSeconds;
  if (measuredTime > 0.0f) {
    const float measuredSpeed = (TRACK_LENGTH_M - 2.0f * SAFE_MARGIN_M) / measuredTime;
    // PWM versus real speed is only approximated as linear between measurements.
    return measuredSpeed * speedPercent / TRAVEL_CALIBRATION_SPEED_PERCENT;
  }
  return UNCALIBRATED_SPEED_AT_100_PERCENT_MPS * speedPercent / 100.0f;
}

float estimatedTravelSeconds(uint8_t index, float from, float to, uint8_t speedPercent) {
  const float speed = estimatedSpeedMps(index, to >= from, speedPercent);
  return speed > 0.0f ? fabsf(to - from) / speed : 0.0f;
}
