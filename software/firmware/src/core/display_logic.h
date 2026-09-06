#pragma once

#include <cstdint>

#include "snapshot.h"

// Turns a StateSnapshot into a 32x16 frame.
//
// Colour carries the light state. Article 11.3.1 makes the digital clock
// authoritative if the clock and the lights ever disagree, so the panel derives
// its timer colour from the same snapshot that produced the digits. AB/CD
// letters default to white and can follow the timer or use a colour from
// settings.
//
// What the panel shows is selectable from the web UI. The clock is the default;
// the rest exist because a 32x16 panel can only hold one thing at a time and a
// director may want the end or arrow count visible to the line instead.

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
  bool clockSeconds = false;
  bool showAbcd = true;
  bool abcdVertical = true;
  bool showEndLabels = true;
  bool abcdFollowTimer = false;
  uint32_t abcdColour = COLOUR_WHITE;
};

struct RenderResult {
  uint32_t checksum = 0;   // identifies the frame without logging 512 pixels
  uint16_t litPixels = 0;
  char text[16] = {0};     // what a person reading the panel would see
};

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

}  // namespace DisplayLogic
