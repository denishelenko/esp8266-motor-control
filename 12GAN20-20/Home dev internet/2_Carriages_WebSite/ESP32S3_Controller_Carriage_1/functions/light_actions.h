#pragma once

bool setLightMode(CarriageState &c, LightMode mode) {
  if (uint8_t(mode) > uint8_t(LightMode::BlinkRed)) return false;
  c.lightMode = mode;
  return true;
}
