#pragma once

// WebSocket JSON is converted to bounded, typed steps before execution.
// No text is interpreted as C++ and no firmware update is involved.
bool decodeScenario(JsonDocument &doc,
                    ScenarioStep (&steps)[2][ScenarioEngine::MAX_STEPS],
                    uint8_t (&counts)[2], uint32_t &id) {
  const int64_t rawId = doc["id"] | int64_t(0);
  if (rawId < 1 || rawId > UINT32_MAX) return false;
  id = uint32_t(rawId);
  JsonArray lanes = doc["lanes"].as<JsonArray>();
  if (id == 0 || lanes.size() != 2) return false;
  for (uint8_t lane = 0; lane < 2; ++lane) {
    JsonArray source = lanes[lane].as<JsonArray>();
    if (source.isNull() || source.size() > ScenarioEngine::MAX_STEPS) return false;
    counts[lane] = uint8_t(source.size());
    for (uint8_t j = 0; j < counts[lane]; ++j) {
      JsonObject item = source[j].as<JsonObject>();
      if (item.isNull()) return false;
      ScenarioStep &s = steps[lane][j];
      const char *action = item["action"] | "";
      if (!strcmp(action, "move_to")) s.action = ScenarioAction::MoveTo;
      else if (!strcmp(action, "move_for")) s.action = ScenarioAction::MoveFor;
      else if (!strcmp(action, "servo")) s.action = ScenarioAction::Servo;
      else if (!strcmp(action, "servo_cycle_start")) s.action = ScenarioAction::ServoCycleStart;
      else if (!strcmp(action, "servo_off")) s.action = ScenarioAction::ServoOff;
      else if (!strcmp(action, "light")) s.action = ScenarioAction::Light;
      else if (!strcmp(action, "wait_ms")) s.action = ScenarioAction::WaitMs;
      else if (!strcmp(action, "wait_peer")) s.action = ScenarioAction::WaitPeer;
      else if (!strcmp(action, "wait_event")) s.action = ScenarioAction::WaitEvent;
      else if (!strcmp(action, "emit_event")) s.action = ScenarioAction::EmitEvent;
      else if (!strcmp(action, "stop_motor")) s.action = ScenarioAction::StopMotor;
      else return false;
      s.meters = item["meters"] | 0.0f;
      const int64_t speed = item["speed"] | int64_t(50);
      const int64_t direction = item["direction"] | int64_t(0);
      const int64_t angle = item["angle"] | int64_t(0);
      if (speed < 0 || speed > 100 || direction < -1 || direction > 1 ||
          angle < 0 || angle > 90) return false;
      s.speed = uint8_t(speed);
      s.direction = int8_t(direction);
      s.angle = uint8_t(angle);
      const char *light = item["mode"] | "auto";
      if (!strcmp(light, "auto")) s.light = LightMode::Auto;
      else if (!strcmp(light, "off")) s.light = LightMode::Off;
      else if (!strcmp(light, "red")) s.light = LightMode::Red;
      else if (!strcmp(light, "green")) s.light = LightMode::Green;
      else if (!strcmp(light, "blink_red")) s.light = LightMode::BlinkRed;
      else return false;
      const int64_t duration = item["durationMs"] | int64_t(0);
      const int64_t closedHold = item["closedHoldMs"] | int64_t(0);
      if (duration < 0 || duration > 300000 || closedHold < 0 || closedHold > 300000)
        return false;
      s.durationMs = uint32_t(duration);
      s.closedHoldMs = uint32_t(closedHold);
      if (item["timeoutMs"].isNull()) {
        const uint32_t extra = s.action == ScenarioAction::Servo ? 15000 : 1000;
        s.timeoutMs = (s.action == ScenarioAction::WaitMs ||
                       s.action == ScenarioAction::MoveFor ||
                       s.action == ScenarioAction::Servo)
            ? min(uint32_t(300000), s.durationMs + extra) : 120000;
      } else {
        const int64_t timeout = item["timeoutMs"].as<int64_t>();
        if (timeout < 1 || timeout > 300000) return false;
        s.timeoutMs = uint32_t(timeout);
      }
      const int64_t peer = item["peer"] | int64_t(0);
      const int64_t peerCompleted = item["peerCompleted"] | int64_t(0);
      const int64_t eventId = item["eventId"] | int64_t(0);
      if (peer < 0 || peer > 2 || peerCompleted < 0 || peerCompleted > 255 ||
          eventId < 0 || eventId > 63) return false;
      s.peer = peer > 0 ? uint8_t(peer - 1) : uint8_t(255);
      s.peerCompleted = uint8_t(peerCompleted);
      s.eventId = uint8_t(eventId);
    }
  }
  return true;
}

const char *scenarioStateName(ScenarioEngine::State state) {
  switch (state) {
    case ScenarioEngine::State::Ready: return "ready";
    case ScenarioEngine::State::Running: return "running";
    case ScenarioEngine::State::Complete: return "complete";
    case ScenarioEngine::State::Stopped: return "stopped";
    case ScenarioEngine::State::Failed: return "failed";
    default: return "empty";
  }
}

const char *scenarioErrorName(ScenarioEngine::Error error) {
  switch (error) {
    case ScenarioEngine::Error::InvalidPlan: return "invalid_plan";
    case ScenarioEngine::Error::RemoteOffline: return "remote_offline";
    case ScenarioEngine::Error::Timeout: return "timeout";
    default: return "none";
  }
}
