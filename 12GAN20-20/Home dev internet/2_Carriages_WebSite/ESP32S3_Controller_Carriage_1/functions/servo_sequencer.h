#pragma once

#include <stdint.h>

// One step commands a physical angle immediately. The hold starts only after
// the estimated travel time has elapsed. Zero travelMs uses calibration.
struct ServoStep {
  uint8_t angle;
  uint32_t holdMs;
  uint32_t travelMs = 0;
};

class ServoSequencer {
 public:
  static constexpr uint8_t MAX_STEPS = 8;

  bool start(const ServoStep *steps, uint8_t count, bool repeat, uint32_t now,
             uint16_t opening90Ms, uint16_t closing90Ms) {
    if (!steps || !count || count > MAX_STEPS) return false;
    for (uint8_t i = 0; i < count; ++i) {
      if (steps[i].angle > 90 || steps[i].holdMs >= 0x80000000UL ||
          steps[i].travelMs >= 0x80000000UL) return false;
    }
    const uint8_t fromAngle = estimatedAngle(now);
    for (uint8_t i = 0; i < count; ++i) steps_[i] = steps[i];
    count_ = count;
    index_ = 0;
    repeat_ = repeat;
    enabled_ = true;
    opening90Ms_ = opening90Ms;
    closing90Ms_ = closing90Ms;
    beginStep(now, fromAngle);
    return true;
  }

  void stop() {
    enabled_ = false;
    phase_ = Phase::Off;
    angle_ = 0;
    count_ = 0;
  }

  void tick(uint32_t now) {
    if (!enabled_) return;
    if (phase_ == Phase::Travel) {
      if (uint32_t(now - phaseAt_) < travelDuration_) return;
      phaseAt_ += travelDuration_;
      phase_ = Phase::Hold;
    }
    if (uint32_t(now - phaseAt_) < steps_[index_].holdMs) return;
    if (index_ + 1 < count_) {
      ++index_;
    } else if (repeat_) {
      index_ = 0;
    } else {
      enabled_ = false;
      phase_ = Phase::Off;
      return;  // A one-shot sequence keeps its last commanded angle.
    }
    beginStep(now, angle_);
  }

  bool active() const { return enabled_; }
  uint8_t commandAngle() const { return angle_; }
  bool travelling() const { return phase_ == Phase::Travel; }
  bool holding() const { return phase_ == Phase::Hold; }

 private:
  enum class Phase : uint8_t { Off, Travel, Hold };
  ServoStep steps_[MAX_STEPS] = {};
  uint8_t count_ = 0, index_ = 0, angle_ = 0, fromAngle_ = 0;
  uint16_t opening90Ms_ = 0, closing90Ms_ = 0;
  uint32_t phaseAt_ = 0, travelDuration_ = 0;
  bool enabled_ = false, repeat_ = false;
  Phase phase_ = Phase::Off;

  uint8_t estimatedAngle(uint32_t now) const {
    if (phase_ != Phase::Travel || !travelDuration_) return angle_;
    const uint32_t elapsed = uint32_t(now - phaseAt_);
    if (elapsed >= travelDuration_) return angle_;
    const int32_t difference = int32_t(angle_) - int32_t(fromAngle_);
    return uint8_t(int32_t(fromAngle_) + int64_t(difference) * elapsed / travelDuration_);
  }

  void beginStep(uint32_t now, uint8_t fromAngle) {
    fromAngle_ = fromAngle;
    angle_ = steps_[index_].angle;
    const uint8_t degrees = angle_ > fromAngle ? angle_ - fromAngle : fromAngle - angle_;
    const uint16_t measured90 = angle_ >= fromAngle ? opening90Ms_ : closing90Ms_;
    travelDuration_ = degrees == 0 ? 0 :
        (steps_[index_].travelMs ? steps_[index_].travelMs :
         (uint32_t(measured90) * degrees + 89U) / 90U);
    phaseAt_ = now;
    phase_ = travelDuration_ ? Phase::Travel : Phase::Hold;
  }
};
