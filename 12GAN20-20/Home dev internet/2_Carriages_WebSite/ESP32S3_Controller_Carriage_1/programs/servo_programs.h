#pragma once

// Program choices are separate from the nonblocking servo engine. Each step
// has its own hold time; the engine adds the measured travel time before it.
constexpr uint32_t DEFAULT_OPEN_HOLD_MS = 2500;
constexpr uint32_t DEFAULT_CLOSED_HOLD_MS = 2500;

bool startServoSteps(CarriageState &c, uint8_t carriageIndex,
                     const ServoStep *steps, uint8_t count, bool repeat,
                     uint32_t now) {
  const bool started = c.servo.start(steps, count, repeat, now,
      SERVO_TRAVEL_90_MS[carriageIndex], SERVO_RETURN_90_MS[carriageIndex]);
  if (started) {
    c.servoEnabled = true;
    c.servoPosition = c.servo.commandAngle();
  }
  return started;
}

// A single command without a hold timer: reach the angle and stay there.
void moveServoTo(CarriageState &c, uint8_t carriageIndex,
                 uint8_t angle, uint32_t now) {
  const ServoStep step[] = {{angle, 0}};
  startServoSteps(c, carriageIndex, step, 1, false, now);
}

// A single command with a hold timer. The timer excludes estimated travel.
void holdServoAt(CarriageState &c, uint8_t carriageIndex,
                 uint8_t angle, uint32_t holdMs, uint32_t now) {
  const ServoStep step[] = {{angle, holdMs}};
  startServoSteps(c, carriageIndex, step, 1, false, now);
}

void startServoCycle(CarriageState &c, uint8_t carriageIndex,
                     uint8_t openAngle, uint32_t openHoldMs,
                     uint32_t closedHoldMs, uint32_t now) {
  if (openAngle > 90) return;
  const ServoStep steps[2] = {
      {c.servoEnabled && c.servoPosition != 0 ? openAngle : uint8_t(0),
       c.servoEnabled && c.servoPosition != 0 ? openHoldMs : closedHoldMs},
      {c.servoEnabled && c.servoPosition != 0 ? uint8_t(0) : openAngle,
       c.servoEnabled && c.servoPosition != 0 ? closedHoldMs : openHoldMs}};
  if (startServoSteps(c, carriageIndex, steps, 2, true, now)) c.servoAngle = openAngle;
}
