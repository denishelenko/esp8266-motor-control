#pragma once

// Time-based movement and position estimation; sensor feedback can replace the estimator.

const char *modeName(MotionMode mode) {
  switch (mode) {
    case MotionMode::Target: return "ціль";
    case MotionMode::Program: return "програма";
    case MotionMode::Jog: return "ручний рух";
    case MotionMode::TimedTest: return "тест часу";
    default: return "стоп";
  }
}

void closeServo(CarriageState &c) {
  c.servoPosition = 0;
  c.nextServoAt = 0;
  c.servoEnabled = false;
}

void selectNextProgramTarget(uint8_t index);

void startProgram(uint8_t index) {
  CarriageState &c = carriages[index];
  if (c.program < 1 || c.program > 4) c.program = 1;
  c.programStep = 0;
  c.nextProgramAt = 0;
  c.ladderRight = true;
  c.mode = MotionMode::Program;
  if (!c.servoEnabled) {
    c.servoEnabled = true;
    c.servoPosition = 0;
    c.nextServoAt = millis() + SERVO_CLOSED_HOLD_MS;
  }
  selectNextProgramTarget(index);
}

void setCarriageTarget(uint8_t index, float target, uint8_t speed) {
  CarriageState &c = carriages[index];
  c.target = constrain(target, -TRACK_LIMIT_M, TRACK_LIMIT_M);
  c.speed = constrain(speed, uint8_t(1), uint8_t(100));
  closeServo(c);
  c.mode = MotionMode::Target;
}

void stopCarriage(uint8_t index, bool keepServo = false) {
  CarriageState &c = carriages[index];
  c.target = c.position;
  c.outputPwm = 0;
  if (!keepServo) closeServo(c);
  c.mode = MotionMode::Idle;
}

// A stopwatch test: no invented metres or position estimate can end the run.
// The existing motor ramp and radio link watchdog remain active.
void startTimedTest(uint8_t index, int8_t direction, uint8_t speed, uint32_t durationMs) {
  stopCarriage(index);
  CarriageState &c = carriages[index];
  c.speed = speed;
  c.testDirection = direction;
  c.testDurationMs = durationMs;
  c.testStartedAt = millis();
  c.mode = MotionMode::TimedTest;
}

template <typename Action>
void applyToSelected(uint8_t selected, Action action) {
  action(selected);
  if (carriagesLinked) action(selected == 0 ? 1 : 0);
}

void selectNextProgramTarget(uint8_t index) {
  CarriageState &c = carriages[index];
  const float l = TRACK_LIMIT_M;
  const uint32_t step = c.programStep++;
  switch (c.program) {
    case 1: c.target = (step & 1) ? -l : l; break; // маятник: край до краю
    case 2: { // драбинка: сусідня позначка через 2 м, зі зміною напрямку на краю
      if (c.position >= l - POSITION_TOLERANCE_M) c.ladderRight = false;
      if (c.position <= -l + POSITION_TOLERANCE_M) c.ladderRight = true;
      constexpr float spacing = 2.0f;
      const float nextMark = c.ladderRight
          ? floorf((c.position + POSITION_TOLERANCE_M) / spacing) + 1.0f
          : ceilf((c.position - POSITION_TOLERANCE_M) / spacing) - 1.0f;
      c.target = constrain(nextMark * spacing, -l, l);
      break;
    }
    case 3: { // ривки: короткі хаотичні стрибки на 1–3 м
      const float jump = 1.0f + float(random(0, 21)) / 10.0f;
      int direction = random(0, 2) ? 1 : -1;
      float candidate = c.position + direction * jump;
      if (candidate > l || candidate < -l) {
        direction = -direction;
        candidate = c.position + direction * jump;
      }
      c.target = constrain(candidate, -l, l);
      break;
    }
    default: { // рандом: довгі переходи, орієнтовно не менше 4 м
      const float distance = 4.0f + float(random(0, 31)) / 10.0f;
      int direction = random(0, 2) ? 1 : -1;
      float candidate = c.position + direction * distance;
      if (candidate > l || candidate < -l) {
        direction = -direction;
        candidate = c.position + direction * distance;
      }
      c.target = constrain(candidate, -l, l);
      break;
    }
  }
  c.mode = MotionMode::Program;
}

void updateCarriage(uint8_t index) {
  CarriageState &c = carriages[index];
  const uint32_t now = millis();
  if (!c.lastTick) c.lastTick = now;
  const float dt = min(0.10f, (now - c.lastTick) / 1000.0f);
  c.lastTick = now;

  if (c.servoEnabled && c.nextServoAt && int32_t(now - c.nextServoAt) >= 0) {
    c.servoPosition = c.servoPosition ? 0 : c.servoAngle;
    c.nextServoAt = now + (c.servoPosition ? SERVO_OPEN_HOLD_MS : SERVO_CLOSED_HOLD_MS);
  }

  if (c.mode == MotionMode::Jog && now - c.lastJogAt >= JOG_TIMEOUT_MS) {
    stopCarriage(index, true);
  }
  if (c.mode == MotionMode::TimedTest) {
    if (now - c.testStartedAt >= c.testDurationMs) {
      stopCarriage(index);
    } else {
      c.outputPwm = int16_t(c.testDirection * map(c.speed, 0, 100, 0, PWM_MAX));
    }
    return;
  }
  if (c.mode == MotionMode::Program && fabsf(c.target - c.position) <= POSITION_TOLERANCE_M) {
    const uint32_t pauseMs = c.program == 1 ? 0 : c.program == 2 ? LADDER_PAUSE_MS
                                             : c.program == 3 ? JERK_PAUSE_MS : RANDOM_PAUSE_MS;
    if (pauseMs && !c.nextProgramAt) c.nextProgramAt = now + pauseMs;
    if (!pauseMs || int32_t(now - c.nextProgramAt) >= 0) {
      selectNextProgramTarget(index);
      c.nextProgramAt = 0;
    }
  }
  const float delta = c.target - c.position;
  if (c.mode == MotionMode::Idle || fabsf(delta) <= POSITION_TOLERANCE_M) {
    c.outputPwm = 0;
    if (c.mode == MotionMode::Target) c.mode = MotionMode::Idle;
    return;
  }
  const float direction = delta > 0 ? 1.0f : -1.0f;
  const uint8_t motionSpeed = c.mode == MotionMode::Program && c.program == 3 ? 100 : c.speed;
  const float speedMps = estimatedSpeedMps(index, direction > 0, motionSpeed);
  const float next = c.position + direction * speedMps * dt;
  c.position = direction > 0 ? min(next, c.target) : max(next, c.target);
  c.outputPwm = int16_t(direction * map(motionSpeed, 0, 100, 0, PWM_MAX));
}
