#pragma once

#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "core/display_logic.h"
#include "core/snapshot.h"

class Display {
public:
  void begin();
  // Draws the snapshot and returns what was drawn, so the caller can trace it.
  DisplayLogic::RenderResult render(const Core::StateSnapshot& state, uint8_t brightness,
                                    const DisplayLogic::RenderRequest& style);

private:
  uint16_t ledIndex(uint8_t x, uint8_t y) const;
  Adafruit_NeoPixel leds_{Config::LED_COUNT, Config::DATA_PIN, NEO_GRB + NEO_KHZ800};
};
