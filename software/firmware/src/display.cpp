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
  // Match the YS-P5 example: each 64x32 module maps onto a 128x16 DMA surface.
  HUB75_I2S_CFG mxconfig(64 * 2, 32 / 2, canvas.chain, pins);
  mxconfig.double_buff = true;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.driver = HUB75_I2S_CFG::FM6124;
  mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595;
  mxconfig.clkphase = false;
  dma_ = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_->begin()) {
    delete dma_;
    dma_ = nullptr;
    return;
  }
  hasFrame_ = false;
  hasBrightness_ = false;
  dma_->clearScreen();
  virtual_ = new VirtualMatrixPanel_T<CHAIN_TOP_LEFT_DOWN, FourScanMapping>(1, canvas.chain, 64, 32);
  virtual_->setDisplay(*dma_);
#else
  const uint16_t count = DisplayLogic::pixelCount(geometry);
  leds_.updateLength(count == 0 ? Config::LED_COUNT : count);
#endif
}

void Display::showFrame(const DisplayLogic::RenderRequest& request, const uint32_t* frame,
                        uint8_t brightness) {
#ifdef DISPLAY_HUB75
  if (dma_ == nullptr || virtual_ == nullptr) return;
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
      // Compensate for the panel's measured red-to-green, green-to-blue, blue-to-red order.
      virtual_->drawPixelRGB888(static_cast<int16_t>(dmaX), static_cast<int16_t>(dmaY),
                static_cast<uint8_t>(colour >> 8), static_cast<uint8_t>(colour),
                static_cast<uint8_t>(colour >> 16));
    }
  }
  dma_->flipDMABuffer();
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
#ifdef DISPLAY_HUB75
  if (dma_ != nullptr && virtual_ != nullptr) {
    if (!hasBrightness_ || brightness != lastBrightness_) {
      dma_->setBrightness8(brightness);
      lastBrightness_ = brightness;
      hasBrightness_ = true;
    }
    if (!hasFrame_ || result.checksum != lastFrameChecksum_) {
      showFrame(request, frame, brightness);
      lastFrameChecksum_ = result.checksum;
      hasFrame_ = true;
    }
  }
#else
  showFrame(request, frame, brightness);
#endif
  return result;
}
