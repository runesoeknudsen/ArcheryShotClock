#pragma once

#include <cstdint>

#include "display_geometry.h"
#include "snapshot.h"

// Turns a StateSnapshot into a panel frame.
//
// The original clock is a 32x16 bitmap. Larger HUB75 and WS2812B layouts
// stamp generated typeface outlines at the cabinet resolution, so a curve
// reads as a hill instead of a staircase of upscaled pixels.
//
// Colour carries the light state. Article 11.3.1 makes the digital clock
// authoritative if the clock and the lights ever disagree, so the panel derives
// its timer colour from the same snapshot that produced the digits. AB/CD
// letters default to white and can follow the timer or use a colour from
// settings.
//
// What each line shows is selectable from the web UI. The clock is the default;
// extra lines exist because a 64x32 or taller panel can hold more than one
// thing at a time.

namespace DisplayLogic {

constexpr uint8_t COLUMNS = 32;
constexpr uint8_t ROWS = 16;
constexpr uint8_t PANEL_ROWS = 8;
constexpr uint8_t PANEL_COLUMNS = 32;
constexpr uint8_t PANEL_ORDER[2] = {1, 0};
constexpr uint16_t PIXEL_COUNT = 512;

// Packed 0xRRGGBB. Green is held below full scale because a WS2812B green
// channel at full output is painfully bright next to the other two and reads
// as white at distance.
constexpr uint32_t COLOUR_RED = 0xFF1808;
constexpr uint32_t COLOUR_GREEN = 0x00C81E;
constexpr uint32_t COLOUR_YELLOW = 0xFFB400;

// Idle is deliberately not amber. The amber this project started with sits
// about nine degrees of hue from the warning yellow, and through a WS2812 with
// bloom, at range, in sunlight, the two are the same colour - so a panel doing
// nothing could read as "thirty seconds left". White is a fourth colour that
// carries no Article 11.3.1 meaning and cannot be confused with any of the
// three that do. Blue is held down because a WS2812 blue channel at full
// output makes white look violet.
constexpr uint32_t COLOUR_IDLE = 0xFFFFC8;

// Default AB/CD letters. Distinct from the timer so the group stays readable
// when the clock is green or red, and still clearly not a signal colour.
constexpr uint32_t COLOUR_WHITE = 0xFFFFFF;

// Same right edge as the MM:SS ones digit, so switching format does not jump.
constexpr uint8_t CLOCK_ONES_LEFT = 25;

// Occupancy ids for the one-LED gap rule between distinct panel elements.
constexpr uint8_t ELEMENT_NONE = 0;
constexpr uint8_t ELEMENT_TIME = 1;
constexpr uint8_t ELEMENT_LABEL = 2;
constexpr uint8_t ELEMENT_GROUP = 3;

struct RenderRequest {
  Core::DisplayContent content = Core::DisplayContent::Clock;
  Core::Light light = Core::Light::Off;
  Core::Phase phase = Core::Phase::Idle;
  uint32_t remainingMs = 0;
  uint16_t endNumber = 0;
  uint8_t arrowsShot = 0;
  uint8_t arrowsPerEnd = 0;
  uint16_t score[2] = {0, 0};
  uint8_t setPoints[2] = {0, 0};
  uint8_t shooter = 0;
  uint8_t detail = 1;
  uint8_t details = 1;
  uint8_t waves = 1;
  bool clockSeconds = false;
  bool showAbcd = true;
  bool abcdVertical = true;
  bool showEndLabels = true;
  bool abcdFollowTimer = false;
  uint32_t abcdColour = COLOUR_WHITE;
  Geometry geometry;
  uint8_t lineCount = 0;
  Core::DisplayContent lines[MAX_CONTENT_LINES] = {};
  LineScaleMode lineScale = LineScaleMode::Fill;
  uint8_t heroLine = 0;
};

struct RenderResult {
  uint32_t checksum = 0;   // identifies the frame without logging every pixel
  uint16_t litPixels = 0;
  char text[64] = {0};     // what a person reading the panel would see
};

bool usesWired32x16(const RenderRequest& request);
uint16_t logicalIndex(uint16_t x, uint16_t y, uint16_t columns);
void applyLayout(RenderRequest& request, PanelPreset preset, Orientation orientation, const uint8_t* lines,
                 uint8_t lineCount, LineScaleMode scaleMode = LineScaleMode::Fill, uint8_t heroLine = 0);

uint16_t ledIndex(uint8_t x, uint8_t y);

uint32_t colourFor(Core::Light light);
uint32_t groupColour(const RenderRequest& request, uint32_t timerColour);
uint32_t parseCssColour(const char* text, uint32_t fallback);
void formatCssColour(uint32_t colour, char* out, uint8_t outSize);

// False for content that has no data behind it yet, so the web UI can offer
// only what the panel can actually show rather than a menu of dead options.
bool contentAvailable(Core::DisplayContent content);

void fillFromSnapshot(RenderRequest& request, const Core::StateSnapshot& state);

// Fills pixels and returns what was drawn. Content with no data falls back to
// the clock rather than showing a blank panel.
RenderResult renderFrame(const RenderRequest& request, uint32_t* pixels);

// True when every pair of distinct drawn elements (time vs BREAK/END/SCORE,
// or AB/CD vs time) has at least one unused LED between occupied pixels,
// including diagonally. Independent of the current font and panel size as
// long as drawing goes through the occupancy map.
bool lastFrameDistinctElementsSeparated();

}  // namespace DisplayLogic
