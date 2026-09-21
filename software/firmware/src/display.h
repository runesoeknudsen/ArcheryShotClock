#pragma once

#ifdef DISPLAY_HUB75
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#else
#include <Adafruit_NeoPixel.h>
#endif

#include "config.h"
#include "core/display_logic.h"
#include "core/snapshot.h"

class Display {
public:
  void begin();
  void configure(const DisplayLogic::Geometry& geometry);
  // Draws the snapshot and returns what was drawn, so the caller can trace it.
  DisplayLogic::RenderResult render(const Core::StateSnapshot& state, uint8_t brightness,
                                    const DisplayLogic::RenderRequest& style);

private:
  void showFrame(const DisplayLogic::RenderRequest& request, const uint32_t* frame, uint8_t brightness);

  DisplayLogic::Geometry geometry_{};
#ifdef DISPLAY_HUB75
  MatrixPanel_I2S_DMA* dma_ = nullptr;
#else
  Adafruit_NeoPixel leds_{Config::LED_COUNT, Config::DATA_PIN, NEO_GRB + NEO_KHZ800};
#endif
};
