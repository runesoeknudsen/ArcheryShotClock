// Adapted from ESP32-HUB75-MatrixPanel-DMA/examples/Pixel_Mapping_Test.
#include "ESP32-VirtualMatrixPanel-I2S-DMA.h"

class CustomPxBasePanel : public VirtualMatrixPanel {
public:
  using VirtualMatrixPanel::VirtualMatrixPanel;

protected:
  VirtualCoords getCoords(int16_t x, int16_t y);
};

inline VirtualCoords CustomPxBasePanel::getCoords(int16_t x, int16_t y) {
  coords = VirtualMatrixPanel::getCoords(x, y);
  if (coords.x == -1 || coords.y == -1) return coords;

  const uint8_t pixelBase = panelResX;
  if (panelResY == 32) {
    if ((coords.y & 8) == 0) {
      coords.x += ((coords.x / pixelBase) + 1) * pixelBase;
    } else {
      coords.x += (coords.x / pixelBase) * pixelBase;
    }
    coords.y = (coords.y >> 4) * 8 + (coords.y & 0b00000111);
  } else if (panelResY == 16) {
    if ((coords.y & 4) == 0) {
      coords.x += ((coords.x / pixelBase) + 1) * pixelBase;
    } else {
      coords.x += (coords.x / pixelBase) * pixelBase;
    }
    coords.y = (coords.y >> 3) * 4 + (coords.y & 0b00000011);
  } else {
    const uint8_t halfHeight = panelResY / 2;
    if ((coords.y % halfHeight) < halfHeight / 2) {
      coords.x += (coords.x / pixelBase + 1) * pixelBase;
    } else {
      coords.x += (coords.x / pixelBase) * pixelBase;
    }
    coords.y = (coords.y / halfHeight) * (halfHeight / 2) +
               (coords.y % (halfHeight / 2));
  }
  return coords;
}

// Physical panel dimensions. The DMA buffer remains twice as wide and half
// as high, matching the upstream example's required constructor dimensions.
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define NUM_ROWS 1
#define NUM_COLS 1
#define VIRTUAL_MATRIX_CHAIN_TYPE CHAIN_BOTTOM_RIGHT_UP

MatrixPanel_I2S_DMA* dmaDisplay = nullptr;
CustomPxBasePanel* fourScanPanel = nullptr;

void setup() {
  Serial.begin(921600);

  HUB75_I2S_CFG::i2s_pins pins = {
      4,  5,  6,  7,  15, 16, 18, 8, 3, -1, -1, 40, 2, 41};
  HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2, PANEL_RES_Y / 2,
                         NUM_ROWS * NUM_COLS, pins);

  mxconfig.clkphase = true;
  mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595;

  dmaDisplay = new MatrixPanel_I2S_DMA(mxconfig);
  dmaDisplay->setBrightness8(40);
  if (!dmaDisplay->begin()) {
    Serial.println("HUB75 DMA memory allocation failed");
  }
  dmaDisplay->clearScreen();
  delay(500);

  fourScanPanel = new CustomPxBasePanel(
      *dmaDisplay, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y,
      VIRTUAL_MATRIX_CHAIN_TYPE);
}

void loop() {
  for (int y = 0; y < fourScanPanel->height(); y++) {
    for (int x = 0; x < fourScanPanel->width(); x++) {
      fourScanPanel->drawPixel(x, y, fourScanPanel->color565(0, 0, 255));
      delay(30);
    }
  }
  delay(2000);
  dmaDisplay->clearScreen();
}