#pragma once

#include <cstdint>

#include "snapshot.h"

// Named panel sizes and how a 32x16 content tile is placed on them.
//
// The original clock is two 8x32 WS2812B modules (32x16). P5 HUB75 modules are
// 64x32. Three of those in a rectangle are 96x64; four in a 2x2 are 128x64.
// Portrait swaps the axes so a tall cabinet can show more stacked lines.

namespace DisplayLogic {

enum class PanelPreset : uint8_t {
  Led32x16 = 0,
  P5_64x32 = 1,
  P5_96x64 = 2,
  P5_128x64 = 3
};

enum class Orientation : uint8_t {
  Landscape = 0,
  Portrait = 1
};

constexpr uint8_t TILE_COLUMNS = 32;
constexpr uint8_t TILE_ROWS = 16;
constexpr uint8_t MAX_CONTENT_LINES = 8;
constexpr uint16_t MAX_COLUMNS = 128;
constexpr uint16_t MAX_ROWS = 128;
constexpr uint16_t MAX_PIXEL_COUNT = 128 * 64;

#ifdef DISPLAY_HUB75
constexpr const char* DISPLAY_DRIVER = "HUB75";
#else
constexpr const char* DISPLAY_DRIVER = "WS2812B";
#endif

#ifdef DISPLAY_HUB75
constexpr PanelPreset DEFAULT_PRESET = PanelPreset::P5_64x32;
#else
constexpr PanelPreset DEFAULT_PRESET = PanelPreset::Led32x16;
#endif

struct Geometry {
  PanelPreset preset = PanelPreset::Led32x16;
  Orientation orientation = Orientation::Landscape;
  uint16_t columns = TILE_COLUMNS;
  uint16_t rows = TILE_ROWS;
};

struct ContentLine {
  Core::DisplayContent content = Core::DisplayContent::Clock;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t scale = 1;
};

struct LayoutPlan {
  Geometry geometry;
  uint8_t lineCount = 1;
  ContentLine lines[MAX_CONTENT_LINES];
};

struct Hub75Canvas {
  uint16_t width = 64;
  uint16_t height = 32;
  uint8_t chain = 1;
};

Geometry geometryFor(PanelPreset preset, Orientation orientation);
uint16_t pixelCount(const Geometry& geometry);
uint8_t maxLinesFor(const Geometry& geometry);
uint8_t defaultLineCount(const Geometry& geometry);
uint8_t defaultLines(const Geometry& geometry, Core::DisplayContent* lines, uint8_t maxLines);

bool validPreset(uint8_t value);
bool validOrientation(uint8_t value);
const char* name(PanelPreset preset);
const char* name(Orientation orientation);
bool parsePreset(const char* text, PanelPreset& out);
bool parseOrientation(const char* text, Orientation& out);
bool parseDisplayContent(const char* text, Core::DisplayContent& out);
uint8_t parseLines(const char* text, Core::DisplayContent* lines, uint8_t maxLines);
void formatLines(const Core::DisplayContent* lines, uint8_t count, char* out, uint8_t outSize);

LayoutPlan planLayout(const Geometry& geometry, const Core::DisplayContent* lines, uint8_t lineCount);

Hub75Canvas hub75Canvas(PanelPreset preset);
bool mapLogicalToHub75(const Geometry& geometry, uint16_t x, uint16_t y, uint16_t& dmaX,
                       uint16_t& dmaY);

// Wiring order for chained 32x8 WS2812B modules. The 32x16 default matches
// ledIndex(): panel 0 is the lower module, panel 1 the upper one rotated 180.
uint16_t ws2812Index(const Geometry& geometry, uint16_t x, uint16_t y);

}  // namespace DisplayLogic
