#include "display.h"

#include "config.h"

void Display::begin() {
  leds_.begin();
  leds_.clear();
}

uint16_t Display::ledIndex(uint8_t x, uint8_t y) const { return DisplayLogic::ledIndex(x, y); }

DisplayLogic::RenderResult Display::render(const Core::StateSnapshot& state, uint8_t brightness,
                                           const DisplayLogic::RenderRequest& style) {
  DisplayLogic::RenderRequest request = style;
  DisplayLogic::fillFromSnapshot(request, state);

  static uint32_t frame[DisplayLogic::PIXEL_COUNT];
  const DisplayLogic::RenderResult result = DisplayLogic::renderFrame(request, frame);

  for (uint16_t index = 0; index < DisplayLogic::PIXEL_COUNT; index++) {
    leds_.setPixelColor(index, frame[index]);
  }
  leds_.setBrightness(brightness);
  leds_.show();
  return result;
}
