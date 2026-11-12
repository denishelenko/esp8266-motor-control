#pragma once

// One carriage's estimated state; position is not sensor feedback.
enum class MotionMode : uint8_t { Idle, Target, Program, Jog };
struct CarriageState {
  float position = 0.0f;
  float target = 0.0f;
  uint8_t speed = 50;
  uint8_t program = 1;
  uint8_t servoAngle = SERVO_DEFAULT_ANGLE;
  uint8_t servoPosition = 0;
  bool servoEnabled = false;
  uint32_t nextServoAt = 0;
  int16_t outputPwm = 0;
  MotionMode mode = MotionMode::Idle;
  uint32_t lastTick = 0;
  uint32_t programStep = 0;
  uint32_t nextProgramAt = 0;
  bool ladderRight = true;
  uint32_t lastJogAt = 0;
};
