#include "display.h"

#include "config.h"

void Display::begin() {
#ifdef DISPLAY_HUB75
  configure(DisplayLogic::geometryFor(DisplayLogic::DEFAULT_PRESET, DisplayLogic::Orientation::Landscape));
#else
  leds_.begin();
  leds_.clear();
#endif
}

void Display::configure(const DisplayLogic::Geometry& geometry) {
  if (geometry_.columns == geometry.columns && geometry_.rows == geometry.rows &&
      geometry_.preset == geometry.preset && geometry_.orientation == geometry.orientation &&
#ifdef DISPLAY_HUB75
      dma_ != nullptr && virtual_ != nullptr
#else
      true
#endif
  ) {
    return;
  }
  geometry_ = geometry;

#ifdef DISPLAY_HUB75
  if (virtual_ != nullptr) {
    delete virtual_;
    virtual_ = nullptr;
  }
  if (dma_ != nullptr) {
    delete dma_;
    dma_ = nullptr;
  }
  const DisplayLogic::Hub75Canvas canvas = DisplayLogic::hub75Canvas(geometry.preset);
  HUB75_I2S_CFG::i2s_pins pins = {Config::HUB75_R1,  Config::HUB75_G1, Config::HUB75_B1, Config::HUB75_R2,
                                  Config::HUB75_G2,  Config::HUB75_B2, Config::HUB75_A,  Config::HUB75_B,
                                  Config::HUB75_C,   Config::HUB75_D,  Config::HUB75_E,  Config::HUB75_LAT,
                                  Config::HUB75_OE,  Config::HUB75_CLK};
  // YS-P5 64x32 is 1/8 scan ("four-scan"). The DMA engine sees each module as
  // 128x16; VirtualMatrixPanel maps the 64x32 cabinet back onto that buffer.
  HUB75_I2S_CFG mxconfig(128, 16, canvas.chain, pins);
  mxconfig.driver = HUB75_I2S_CFG::FM6124;
  mxconfig.clkphase = false;
  dma_ = new MatrixPanel_I2S_DMA(mxconfig);
  dma_->begin();
  dma_->clearScreen();
  virtual_ = new VirtualMatrixPanel(*dma_, 1, canvas.chain, 64, 32, CHAIN_NONE);
  virtual_->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
#else
  const uint16_t count = DisplayLogic::pixelCount(geometry);
  leds_.updateLength(count == 0 ? Config::LED_COUNT : count);
#endif
}

void Display::showFrame(const DisplayLogic::RenderRequest& request, const uint32_t* frame,
                        uint8_t brightness) {
#ifdef DISPLAY_HUB75
  if (dma_ == nullptr || virtual_ == nullptr) return;
  dma_->setBrightness8(brightness);
  const bool wired = DisplayLogic::usesWired32x16(request);
  const uint16_t columns = request.geometry.columns;
  const uint16_t rows = request.geometry.rows;
  for (uint16_t y = 0; y < rows; y++) {
    for (uint16_t x = 0; x < columns; x++) {
      const uint32_t colour =
          wired ? frame[DisplayLogic::ledIndex(static_cast<uint8_t>(x), static_cast<uint8_t>(y))]
                : frame[DisplayLogic::logicalIndex(x, y, columns)];
      uint16_t dmaX = x;
      uint16_t dmaY = y;
      if (!DisplayLogic::mapLogicalToHub75(request.geometry, x, y, dmaX, dmaY)) continue;
      virtual_->drawPixelRGB888(static_cast<int16_t>(dmaX), static_cast<int16_t>(dmaY),
                                static_cast<uint8_t>(colour >> 16), static_cast<uint8_t>(colour >> 8),
                                static_cast<uint8_t>(colour));
    }
  }
#else
  if (DisplayLogic::usesWired32x16(request)) {
    for (uint16_t index = 0; index < DisplayLogic::PIXEL_COUNT; index++) {
      leds_.setPixelColor(index, frame[index]);
    }
  } else {
    for (uint16_t y = 0; y < request.geometry.rows; y++) {
      for (uint16_t x = 0; x < request.geometry.columns; x++) {
        const uint16_t logical = DisplayLogic::logicalIndex(x, y, request.geometry.columns);
        leds_.setPixelColor(DisplayLogic::ws2812Index(request.geometry, x, y), frame[logical]);
      }
    }
  }
  leds_.setBrightness(brightness);
  leds_.show();
#endif
}

DisplayLogic::RenderResult Display::render(const Core::StateSnapshot& state, uint8_t brightness,
                                           const DisplayLogic::RenderRequest& style) {
  DisplayLogic::RenderRequest request = style;
  DisplayLogic::fillFromSnapshot(request, state);
  configure(request.geometry);

  static uint32_t frame[DisplayLogic::MAX_PIXEL_COUNT];
  const DisplayLogic::RenderResult result = DisplayLogic::renderFrame(request, frame);
  showFrame(request, frame, brightness);
  return result;
}
