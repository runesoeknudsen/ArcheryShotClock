#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define EXAMPLE_NUMBER 3
#define NUMBER_OF_PANELS 3
//#define PANEL_SCAN_TYPE STANDARD_TWO_SCAN
//#define PANEL_SCAN_TYPE FOUR_SCAN_32PX_HIGH
//#define PANEL_SCAN_TYPE FOUR_SCAN_5PX_64X32HIGH
#define PANEL_SCAN_TYPE FOUR_SCAN_64X32_DOUBLE_WIDTH

#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN

using FourScanMapping = ScanTypeMapping<PANEL_SCAN_TYPE>;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, FourScanMapping>* virtualDisplay = nullptr;
MatrixPanel_I2S_DMA* dmaDisplay = nullptr;

void setup() {
  Serial.begin(921600);
  delay(2000);

  HUB75_I2S_CFG::i2s_pins pins = {
      4, 5, 6, 7, 15, 16, 18, 8, 3, -1, -1, 40, 2, 41};
  HUB75_I2S_CFG config(PANEL_RES_X*2, PANEL_RES_Y/2, NUMBER_OF_PANELS, pins);
  config.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  config.clkphase = false;
  config.driver = HUB75_I2S_CFG::FM6124;
  config.line_decoder = HUB75_I2S_CFG::TYPE595;

  dmaDisplay = new MatrixPanel_I2S_DMA(config);
  if (!dmaDisplay->begin()) {
    Serial.println("HUB75 DMA memory allocation failed");
    return;
  }
  dmaDisplay->setBrightness8(40);
  dmaDisplay->clearScreen();

  virtualDisplay = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, FourScanMapping>(
      NUMBER_OF_PANELS, 1, PANEL_RES_X, PANEL_RES_Y);
  virtualDisplay->setDisplay(*dmaDisplay);


  /*
  while(true) {
    for (int x = 0; x < virtualDisplay->width(); x++) {
      for (int y = 0; y < virtualDisplay->height(); y++) {
        uint16_t color = virtualDisplay->color565(96, 0, 0);
        if (x == 0) color = virtualDisplay->color565(0, 255, 0);
        if (x == virtualDisplay->width() - 1) {
          color = virtualDisplay->color565(0, 0, 255);
        }
        virtualDisplay->drawPixel(x, y, color);
         Serial.println("x=" + String(x) + ", y=" + String(y));
        delay(1000/4);
        virtualDisplay->drawPixel(x, y, virtualDisplay->color565(0, 0, 0));
      }
    }
  }*/

  for (int y = 0; y < virtualDisplay->height(); y++) {
    for (int x = 0; x < virtualDisplay->width(); x++) {
      uint16_t color = virtualDisplay->color565(96, 0, 0);
      if (x == 0) color = virtualDisplay->color565(0, 255, 0);
      if (x == virtualDisplay->width() - 1) {
        color = virtualDisplay->color565(0, 0, 255);
      }
      virtualDisplay->drawPixel(x, y, color);
      delay(1);
      virtualDisplay->drawPixel(x, y, virtualDisplay->color565(0, 0, 0));
    }
  }



  virtualDisplay->drawLine(virtualDisplay->width() - 1,
                           virtualDisplay->height() - 1, 0, 0,
                           virtualDisplay->color565(255, 255, 255));
  virtualDisplay->print("Virtual Matrix Panel");
  delay(3000);
  virtualDisplay->clearScreen();
  virtualDisplay->drawDisplayTest(); //bgr

  // (31,0)
  virtualDisplay->drawPixel(0, 0, virtualDisplay->color565(96, 0, 0));
  // (31, 8)
  virtualDisplay->drawPixel(1, 0, virtualDisplay->color565(0, 96, 0));
  // (31, 9)
  virtualDisplay->drawPixel(0, 1, virtualDisplay->color565(0, 0, 96));

  // (63, 31) Top right corner
  virtualDisplay->drawPixel(0, 31, virtualDisplay->color565(0, 96, 96));
  // (32, 23) Top right corner
  virtualDisplay->drawPixel(63, 31, virtualDisplay->color565(96, 96, 0));
  delay(3000);

  virtualDisplay->fillScreen(virtualDisplay->color565(96, 0, 0));
  delay(3000);
  virtualDisplay->fillScreen(virtualDisplay->color565(0, 96, 0));
  delay(3000);
  virtualDisplay->fillScreen(virtualDisplay->color565(0, 0, 96));
  delay(3000);
  virtualDisplay->fillScreen(virtualDisplay->color565(96, 96, 96));
  delay(3000);

  Serial.println("Enter x,y and press Enter to light a green pixel.");
}

/*void loop() {
  sleep(1000);
}*/



void loop() {
  static String coordinates;
  static int x = 0;
  static int y = 0;
  static bool hasCoordinate = false;
  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\r') continue;

    if (character == '\n') {
      const int separator = coordinates.indexOf(',');
      if (coordinates == "+") {
        if (hasCoordinate && y + 1 < virtualDisplay->height()) {
          y++;
          virtualDisplay->drawPixel(x, y, virtualDisplay->color565(255, 0, 0));
          Serial.println("Lit x=" + String(x) + ", y=" + String(y));
        } else if (!hasCoordinate) {
          Serial.println("Enter x,y before using +.");
        } else {
          Serial.println("No next y coordinate; already at the bottom row.");
        }
      } else if (separator >= 0) {
        const int nextX = coordinates.substring(0, separator).toInt();
        const int nextY = coordinates.substring(separator + 1).toInt();
        if (nextX >= 0 && nextX < virtualDisplay->width() &&
            nextY >= 0 && nextY < virtualDisplay->height()) {
          x = nextX;
          y = nextY;
          hasCoordinate = true;
          virtualDisplay->drawPixel(x, y, virtualDisplay->color565(255, 0, 0));
          Serial.println("Lit x=" + String(x) + ", y=" + String(y));
        } else {
          Serial.println("Coordinates out of range.");
        }
      } else {
        Serial.println("Enter x,y or + (for example: 10,5).");
      }
      coordinates = "";
    } else {
      coordinates += character;
    }
  }
}
