#pragma once

#include <math.h>

enum class ScenarioAction : uint8_t {
  MoveTo, MoveFor, Servo, ServoCycleStart, ServoOff, Light, WaitMs,
  WaitPeer, WaitEvent, EmitEvent, StopMotor
};

struct ScenarioStep {
  ScenarioAction action = ScenarioAction::WaitMs;
  float meters = 0;
  uint8_t speed = 50;
  int8_t direction = 0;
  uint8_t angle = 0;
  LightMode light = LightMode::Auto;
  uint8_t peer = 0;
  uint8_t peerCompleted = 0;
  uint8_t eventId = 0;
  uint32_t durationMs = 0;
  uint32_t closedHoldMs = 0;
  uint32_t timeoutMs = 120000;
};

class ScenarioEngine {
 public:
  static constexpr uint8_t MAX_STEPS = 16;
  enum class State : uint8_t { Empty, Ready, Running, Complete, Stopped, Failed };
  enum class Error : uint8_t { None, InvalidPlan, RemoteOffline, Timeout };

  bool load(const ScenarioStep (&steps)[2][MAX_STEPS], const uint8_t (&counts)[2],
            uint32_t id) {
    if (state_ == State::Running || id == 0 ||
        (counts[0] == 0 && counts[1] == 0) ||
        counts[0] > MAX_STEPS || counts[1] > MAX_STEPS) return false;
    for (uint8_t lane = 0; lane < 2; ++lane) {
      for (uint8_t j = 0; j < counts[lane]; ++j) {
        const ScenarioStep &s = steps[lane][j];
        if (s.timeoutMs == 0 || s.timeoutMs > 300000) return false;
        switch (s.action) {
          case ScenarioAction::MoveTo:
            if (!isfinite(s.meters) || fabsf(s.meters) > TRACK_LIMIT_M ||
                s.speed < 1 || s.speed > 100) return false;
            break;
          case ScenarioAction::MoveFor:
            if ((s.direction != -1 && s.direction != 1) || s.speed < 1 ||
                s.speed > 100 || s.durationMs < 200 || s.durationMs > 120000) return false;
            break;
          case ScenarioAction::Servo:
            if (s.angle > 90 || s.durationMs > 300000) return false;
            break;
          case ScenarioAction::ServoCycleStart:
            if (s.angle < 1 || s.angle > 90 || s.durationMs == 0 ||
                s.durationMs > 300000 || s.closedHoldMs == 0 ||
                s.closedHoldMs > 300000) return false;
            break;
          case ScenarioAction::Light:
            if (uint8_t(s.light) > uint8_t(LightMode::BlinkRed)) return false;
            break;
          case ScenarioAction::WaitMs:
            if (s.durationMs > 300000) return false;
            break;
          case ScenarioAction::WaitPeer:
            if (s.peer > 1 || s.peer == lane || s.peerCompleted == 0 ||
                s.peerCompleted > counts[s.peer]) return false;
            break;
          case ScenarioAction::WaitEvent:
          case ScenarioAction::EmitEvent:
            if (s.eventId == 0 || s.eventId > 63) return false;
            break;
          case ScenarioAction::ServoOff:
          case ScenarioAction::StopMotor:
            break;
          default: return false;
        }
      }
    }
    for (uint8_t lane = 0; lane < 2; ++lane) {
      count_[lane] = counts[lane];
      for (uint8_t j = 0; j < counts[lane]; ++j) steps_[lane][j] = steps[lane][j];
    }
    id_ = id;
    state_ = State::Ready;
    error_ = Error::None;
    return true;
  }

  bool start(uint32_t expectedId, bool remoteOnline) {
    if (expectedId == 0 || expectedId != id_) return false;
    if (state_ != State::Ready && state_ != State::Complete && state_ != State::Stopped) return false;
    if (count_[1] && !remoteOnline) return false;
    for (uint8_t i = 0; i < 2; ++i) {
      stopCarriage(i);
      setLightMode(carriages[i], LightMode::Auto);
      index_[i] = 0;
      entered_[i] = false;
    }
    events_ = 0;
    error_ = Error::None;
    state_ = State::Running;
    return true;
  }

  void stop() {
    if (state_ != State::Running) return;
    for (uint8_t i = 0; i < 2; ++i) {
      stopCarriage(i);
      setLightMode(carriages[i], LightMode::Auto);
    }
    state_ = State::Stopped;
  }

  void signalEvent(uint8_t eventId) {
    if (eventId > 0 && eventId <= 63) events_ |= uint64_t(1) << eventId;
  }

  void tick(uint32_t now, bool remoteOnline, int16_t remoteAppliedPwm,
            uint8_t remoteServoAngle, LightMode remoteLightMode,
            uint32_t commandSequence, uint32_t ackSequence) {
    if (state_ != State::Running) return;
    if (count_[1] && !remoteOnline) { fail(Error::RemoteOffline); return; }
    for (uint8_t lane = 0; lane < 2; ++lane) {
      if (index_[lane] >= count_[lane]) continue;
      const ScenarioStep &s = steps_[lane][index_[lane]];
      CarriageState &c = carriages[lane];
      if (!entered_[lane]) {
        entered_[lane] = true;
        enteredAt_[lane] = now;
        enteredSequence_[lane] = commandSequence;
        remoteMotionSeen_[lane] = false;
        noMotionNeeded_[lane] = s.action == ScenarioAction::MoveTo &&
                                fabsf(c.position - s.meters) <= POSITION_TOLERANCE_M;
        switch (s.action) {
          case ScenarioAction::MoveTo: moveMotorTo(c, s.meters, s.speed); break;
          case ScenarioAction::MoveFor:
            moveMotorFor(c, s.direction, s.speed, s.durationMs, now); break;
          case ScenarioAction::Servo:
            holdServoAt(c, lane, s.angle, s.durationMs, now); break;
          case ScenarioAction::ServoCycleStart:
            startServoCycle(c, lane, s.angle, s.durationMs, s.closedHoldMs, now); break;
          case ScenarioAction::ServoOff: closeServo(c); break;
          case ScenarioAction::Light: setLightMode(c, s.light); break;
          case ScenarioAction::StopMotor: stopMotor(c); break;
          default: break;
        }
      }
      if (uint32_t(now - enteredAt_[lane]) > s.timeoutMs) {
        fail(Error::Timeout);
        return;
      }
      bool done = false;
      const bool remoteAcked = int32_t(ackSequence - enteredSequence_[lane]) > 0;
      if (lane == 1 && remoteAcked && remoteAppliedPwm != 0)
        remoteMotionSeen_[lane] = true;
      switch (s.action) {
        case ScenarioAction::MoveTo:
          done = c.mode == MotionMode::Idle &&
                 fabsf(c.position - s.meters) <= POSITION_TOLERANCE_M &&
                 (lane == 0 || noMotionNeeded_[lane] ||
                  (remoteMotionSeen_[lane] && remoteAcked && remoteAppliedPwm == 0));
          break;
        case ScenarioAction::MoveFor:
          done = c.mode == MotionMode::Idle &&
                 (lane == 0 || (remoteMotionSeen_[lane] && remoteAcked && remoteAppliedPwm == 0));
          break;
        case ScenarioAction::Servo:
          done = !c.servo.active() &&
                 (lane == 0 || (remoteAcked && remoteServoAngle == s.angle));
          break;
        case ScenarioAction::ServoCycleStart: done = lane == 0 || remoteAcked; break;
        case ScenarioAction::WaitMs: done = uint32_t(now - enteredAt_[lane]) >= s.durationMs; break;
        case ScenarioAction::WaitPeer: done = index_[s.peer] >= s.peerCompleted; break;
        case ScenarioAction::WaitEvent: done = (events_ & (uint64_t(1) << s.eventId)) != 0; break;
        case ScenarioAction::EmitEvent: signalEvent(s.eventId); done = true; break;
        case ScenarioAction::ServoOff:
          done = lane == 0 || (remoteAcked && remoteServoAngle == 0); break;
        case ScenarioAction::Light:
          done = lane == 0 || (remoteAcked && remoteLightMode == s.light); break;
        case ScenarioAction::StopMotor:
          done = lane == 0 || (remoteAcked && remoteAppliedPwm == 0); break;
      }
      if (done) { ++index_[lane]; entered_[lane] = false; }
    }
    if (index_[0] >= count_[0] && index_[1] >= count_[1]) state_ = State::Complete;
  }

  State state() const { return state_; }
  Error error() const { return error_; }
  uint32_t id() const { return id_; }
  uint8_t completed(uint8_t lane) const { return lane < 2 ? index_[lane] : 0; }
  uint8_t count(uint8_t lane) const { return lane < 2 ? count_[lane] : 0; }

 private:
  ScenarioStep steps_[2][MAX_STEPS] = {};
  uint8_t count_[2] = {}, index_[2] = {};
  uint32_t enteredAt_[2] = {};
  uint32_t enteredSequence_[2] = {};
  bool entered_[2] = {};
  bool remoteMotionSeen_[2] = {};
  bool noMotionNeeded_[2] = {};
  uint64_t events_ = 0;
  uint32_t id_ = 0;
  State state_ = State::Empty;
  Error error_ = Error::None;

  void fail(Error why) {
    for (uint8_t i = 0; i < 2; ++i) {
      stopCarriage(i);
      setLightMode(carriages[i], LightMode::Auto);
    }
    error_ = why;
    state_ = State::Failed;
  }
};
