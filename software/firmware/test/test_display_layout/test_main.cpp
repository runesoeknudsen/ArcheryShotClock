#include <unity.h>

#include "core/display_logic.h"

namespace {

uint32_t large[DisplayLogic::MAX_PIXEL_COUNT];
uint32_t tile[DisplayLogic::PIXEL_COUNT];

uint32_t logicalAt(uint16_t x, uint16_t y, uint16_t columns) {
  return large[DisplayLogic::logicalIndex(x, y, columns)];
}

}  // namespace

void test_named_sizes_and_portrait_swap() {
  const DisplayLogic::Geometry one =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x32, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_EQUAL_UINT16(64, one.columns);
  TEST_ASSERT_EQUAL_UINT16(32, one.rows);
  TEST_ASSERT_EQUAL_UINT16(2048, DisplayLogic::pixelCount(one));

  const DisplayLogic::Geometry tall =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x32, DisplayLogic::Orientation::Portrait);
  TEST_ASSERT_EQUAL_UINT16(32, tall.columns);
  TEST_ASSERT_EQUAL_UINT16(64, tall.rows);
  TEST_ASSERT_EQUAL_UINT8(8, DisplayLogic::maxLinesFor(tall));
  TEST_ASSERT_EQUAL_UINT8(8, DisplayLogic::defaultLineCount(tall));

  const DisplayLogic::Geometry two =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x64, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_EQUAL_UINT16(64, two.columns);
  TEST_ASSERT_EQUAL_UINT16(64, two.rows);

  const DisplayLogic::Geometry three =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_EQUAL_UINT16(96, three.columns);
  TEST_ASSERT_EQUAL_UINT16(64, three.rows);

  const DisplayLogic::Geometry four =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_128x64, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_EQUAL_UINT16(128, four.columns);
  TEST_ASSERT_EQUAL_UINT16(64, four.rows);

  const DisplayLogic::Geometry five =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_160x64, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_EQUAL_UINT16(160, five.columns);
  TEST_ASSERT_EQUAL_UINT16(64, five.rows);
  const DisplayLogic::Geometry fiveTall =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_160x64, DisplayLogic::Orientation::Portrait);
  TEST_ASSERT_EQUAL_UINT16(64, fiveTall.columns);
  TEST_ASSERT_EQUAL_UINT16(160, fiveTall.rows);

  const DisplayLogic::Geometry led =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::Led32x16, DisplayLogic::Orientation::Portrait);
  TEST_ASSERT_EQUAL_UINT16(32, led.columns);
  TEST_ASSERT_EQUAL_UINT16(16, led.rows);
}

void test_landscape_defaults_to_one_clock_line() {
  const DisplayLogic::Geometry three =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_EQUAL_UINT8(1, DisplayLogic::defaultLineCount(three));
  Core::DisplayContent lines[DisplayLogic::MAX_CONTENT_LINES] = {};
  TEST_ASSERT_EQUAL_UINT8(1, DisplayLogic::defaultLines(three, lines, DisplayLogic::MAX_CONTENT_LINES));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Core::DisplayContent::Clock),
                          static_cast<uint8_t>(lines[0]));
}

void test_64x32_clock_is_not_a_scaled_tile() {
  DisplayLogic::RenderRequest small;
  small.content = Core::DisplayContent::Clock;
  small.light = Core::Light::Green;
  small.remainingMs = 90000;
  DisplayLogic::renderFrame(small, tile);

  DisplayLogic::RenderRequest largeReq;
  largeReq.content = Core::DisplayContent::Clock;
  largeReq.light = Core::Light::Green;
  largeReq.remainingMs = 90000;
  largeReq.geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x32, DisplayLogic::Orientation::Landscape);
  const DisplayLogic::RenderResult result = DisplayLogic::renderFrame(largeReq, large);

  TEST_ASSERT_EQUAL_STRING("01:30", result.text);
  TEST_ASSERT_GREATER_THAN_UINT16(40, result.litPixels);

  bool matchesNearestNeighbour = true;
  for (uint8_t y = 0; y < DisplayLogic::ROWS && matchesNearestNeighbour; y++) {
    for (uint8_t x = 0; x < DisplayLogic::COLUMNS; x++) {
      const uint32_t colour = tile[DisplayLogic::ledIndex(x, y)];
      for (uint8_t dy = 0; dy < 2; dy++) {
        for (uint8_t dx = 0; dx < 2; dx++) {
          if (logicalAt(static_cast<uint16_t>(x * 2 + dx), static_cast<uint16_t>(y * 2 + dy), 64) !=
              colour) {
            matchesNearestNeighbour = false;
          }
        }
      }
    }
  }
  TEST_ASSERT_FALSE(matchesNearestNeighbour);
}

void test_portrait_stacks_more_lines_in_order() {
  DisplayLogic::RenderRequest request;
  request.light = Core::Light::Red;
  request.remainingMs = 20000;
  request.endNumber = 4;
  request.score[0] = 12;
  request.score[1] = 9;
  request.arrowsShot = 2;
  request.arrowsPerEnd = 3;
  request.showEndLabels = false;
  request.geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x32, DisplayLogic::Orientation::Portrait);
  request.lineCount = 3;
  request.lines[0] = Core::DisplayContent::Clock;
  request.lines[1] = Core::DisplayContent::ClockAndEnd;
  request.lines[2] = Core::DisplayContent::Score;

  const DisplayLogic::RenderResult result = DisplayLogic::renderFrame(request, large);
  TEST_ASSERT_EQUAL_STRING("00:20 | 04 | 12-09", result.text);

  const DisplayLogic::LayoutPlan plan =
      DisplayLogic::planLayout(request.geometry, request.lines, request.lineCount);
  TEST_ASSERT_EQUAL_UINT8(3, plan.lineCount);
  TEST_ASSERT_EQUAL_UINT16(0, plan.lines[0].y);
  TEST_ASSERT_EQUAL_UINT16(21, plan.lines[0].height);
  TEST_ASSERT_EQUAL_UINT16(21, plan.lines[1].y);
  TEST_ASSERT_EQUAL_UINT16(21, plan.lines[1].height);
  TEST_ASSERT_EQUAL_UINT16(42, plan.lines[2].y);
  TEST_ASSERT_EQUAL_UINT16(22, plan.lines[2].height);
}

void test_line_list_parsing() {
  Core::DisplayContent lines[DisplayLogic::MAX_CONTENT_LINES] = {};
  TEST_ASSERT_EQUAL_UINT8(3, DisplayLogic::parseLines("CLOCK, SCORE, ARROWS", lines, 8));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Core::DisplayContent::Clock),
                          static_cast<uint8_t>(lines[0]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Core::DisplayContent::Score),
                          static_cast<uint8_t>(lines[1]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Core::DisplayContent::ArrowCount),
                          static_cast<uint8_t>(lines[2]));

  char formatted[40] = {};
  DisplayLogic::formatLines(lines, 3, formatted, sizeof(formatted));
  TEST_ASSERT_EQUAL_STRING("CLOCK,SCORE,ARROWS", formatted);
}

void test_hub75_vertical_panels_are_rotated() {
  const DisplayLogic::Geometry three =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  uint16_t dmaX = 0;
  uint16_t dmaY = 0;
  TEST_ASSERT_TRUE(DisplayLogic::mapLogicalToHub75(three, 0, 0, dmaX, dmaY));
  TEST_ASSERT_EQUAL_UINT16(0, dmaX);
  TEST_ASSERT_EQUAL_UINT16(31, dmaY);
  TEST_ASSERT_TRUE(DisplayLogic::mapLogicalToHub75(three, 32, 0, dmaX, dmaY));
  TEST_ASSERT_EQUAL_UINT16(64, dmaX);
  TEST_ASSERT_EQUAL_UINT16(31, dmaY);
  TEST_ASSERT_TRUE(DisplayLogic::mapLogicalToHub75(three, 95, 63, dmaX, dmaY));
  TEST_ASSERT_EQUAL_UINT16(191, dmaX);
  TEST_ASSERT_EQUAL_UINT16(0, dmaY);
  TEST_ASSERT_EQUAL_UINT8(3, DisplayLogic::hub75Canvas(DisplayLogic::PanelPreset::P5_96x64).chain);

  const DisplayLogic::Geometry four =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_128x64, DisplayLogic::Orientation::Landscape);
  TEST_ASSERT_TRUE(DisplayLogic::mapLogicalToHub75(four, 127, 63, dmaX, dmaY));
  TEST_ASSERT_EQUAL_UINT16(255, dmaX);
  TEST_ASSERT_EQUAL_UINT16(0, dmaY);
  TEST_ASSERT_EQUAL_UINT8(4, DisplayLogic::hub75Canvas(DisplayLogic::PanelPreset::P5_128x64).chain);
  TEST_ASSERT_EQUAL_UINT8(5, DisplayLogic::hub75Canvas(DisplayLogic::PanelPreset::P5_160x64).chain);
}

void test_hub75_three_vertical_uses_every_chain_pixel() {
  const DisplayLogic::Geometry geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  bool used[192 * 32] = {};
  uint16_t dmaX = 0;
  uint16_t dmaY = 0;
  for (uint16_t y = 0; y < 64; y++) {
    for (uint16_t x = 0; x < 96; x++) {
      TEST_ASSERT_TRUE(DisplayLogic::mapLogicalToHub75(geometry, x, y, dmaX, dmaY));
      TEST_ASSERT_LESS_THAN_UINT16(192, dmaX);
      TEST_ASSERT_LESS_THAN_UINT16(32, dmaY);
      const uint16_t index = static_cast<uint16_t>(dmaY * 192 + dmaX);
      TEST_ASSERT_FALSE(used[index]);
      used[index] = true;
    }
  }
}

void test_smooth_glyphs_use_in_between_levels() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Green;
  request.remainingMs = 90000;
  request.geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  request.lineCount = 1;
  request.lines[0] = Core::DisplayContent::Clock;
  DisplayLogic::renderFrame(request, large);

  uint16_t soft = 0;
  uint16_t full = 0;
  const uint16_t count = DisplayLogic::pixelCount(request.geometry);
  for (uint16_t index = 0; index < count; index++) {
    if (large[index] == 0) continue;
    if (large[index] == DisplayLogic::COLOUR_GREEN) full++;
    else soft++;
  }
  TEST_ASSERT_GREATER_THAN_UINT16(20, full);
  TEST_ASSERT_GREATER_THAN_UINT16(10, soft);

  bool foundHill = false;
  for (uint16_t y = 0; y < 64 && !foundHill; y++) {
    bool risingThroughSoft = false;
    for (uint16_t x = 0; x < 96; x++) {
      const uint32_t colour = logicalAt(x, y, 96);
      const uint16_t sum = static_cast<uint16_t>(((colour >> 16) & 0xFFu) + ((colour >> 8) & 0xFFu) +
                                                 (colour & 0xFFu));
      if (sum > 40 && sum < 400 && colour != DisplayLogic::COLOUR_GREEN) {
        risingThroughSoft = true;
      }
      if (risingThroughSoft && colour == DisplayLogic::COLOUR_GREEN) {
        foundHill = true;
        break;
      }
      if (colour == 0) risingThroughSoft = false;
    }
  }
  TEST_ASSERT_TRUE(foundHill);
}

void inkSpan(uint16_t x0, uint16_t x1, uint16_t columns, uint16_t rows, uint16_t* top, uint16_t* bottom) {
  *top = rows;
  *bottom = 0;
  for (uint16_t y = 0; y < rows; y++) {
    for (uint16_t x = x0; x < x1; x++) {
      if (logicalAt(x, y, columns) == 0) continue;
      if (y < *top) *top = y;
      if (y > *bottom) *bottom = y;
    }
  }
}

void test_wide_and_narrow_glyphs_share_one_scale() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Green;
  request.remainingMs = 90000;
  request.geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  request.lineCount = 1;
  request.lines[0] = Core::DisplayContent::Clock;
  DisplayLogic::renderFrame(request, large);

  // 01:30. Digit cells are 5 tile units wide; the 96x64 cabinet is 3 dest
  // pixels per tile column. A shared scale keeps the wide 0 as tall as the 1.
  const uint16_t zeroLeft = 2 * 3;
  const uint16_t oneLeft = 9 * 3;
  const uint16_t cellW = 5 * 3;
  uint16_t zeroTop = 0;
  uint16_t zeroBottom = 0;
  uint16_t oneTop = 0;
  uint16_t oneBottom = 0;
  inkSpan(zeroLeft, static_cast<uint16_t>(zeroLeft + cellW), 96, 64, &zeroTop, &zeroBottom);
  inkSpan(oneLeft, static_cast<uint16_t>(oneLeft + cellW), 96, 64, &oneTop, &oneBottom);
  TEST_ASSERT_LESS_THAN_UINT16(64, zeroTop);
  TEST_ASSERT_LESS_THAN_UINT16(64, oneTop);
  const uint16_t zeroH = static_cast<uint16_t>(zeroBottom - zeroTop + 1);
  const uint16_t oneH = static_cast<uint16_t>(oneBottom - oneTop + 1);
  TEST_ASSERT_GREATER_THAN_UINT16(12, zeroH);
  TEST_ASSERT_GREATER_THAN_UINT16(12, oneH);
  const uint16_t delta = zeroH > oneH ? static_cast<uint16_t>(zeroH - oneH)
                                      : static_cast<uint16_t>(oneH - zeroH);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(4, delta);
}

void test_shooting_seconds_fill_the_large_panel() {
  DisplayLogic::RenderRequest idle;
  idle.content = Core::DisplayContent::Clock;
  idle.light = Core::Light::Green;
  idle.remainingMs = 90000;
  idle.geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_96x64, DisplayLogic::Orientation::Landscape);
  idle.lineCount = 1;
  idle.lines[0] = Core::DisplayContent::Clock;
  DisplayLogic::renderFrame(idle, large);
  uint16_t idleTop = 0;
  uint16_t idleBottom = 0;
  inkSpan(6, 21, 96, 64, &idleTop, &idleBottom);
  const uint16_t idleH = static_cast<uint16_t>(idleBottom - idleTop + 1);

  DisplayLogic::RenderRequest request = idle;
  request.phase = Core::Phase::Shooting;
  request.details = 2;
  request.detail = 1;
  request.showAbcd = true;
  request.abcdVertical = true;
  DisplayLogic::renderFrame(request, large);

  uint16_t tensTop = 0;
  uint16_t tensBottom = 0;
  uint16_t onesTop = 0;
  uint16_t onesBottom = 0;
  uint16_t aTop = 0;
  uint16_t aBottom = 0;
  inkSpan(45, 69, 96, 64, &tensTop, &tensBottom);
  inkSpan(72, 96, 96, 64, &onesTop, &onesBottom);
  inkSpan(0, 15, 96, 64, &aTop, &aBottom);
  const uint16_t tensH = static_cast<uint16_t>(tensBottom - tensTop + 1);
  const uint16_t onesH = static_cast<uint16_t>(onesBottom - onesTop + 1);
  const uint16_t aH = static_cast<uint16_t>(aBottom - aTop + 1);
  TEST_ASSERT_GREATER_THAN_UINT16(idleH, tensH);
  TEST_ASSERT_GREATER_THAN_UINT16(idleH, onesH);
  TEST_ASSERT_GREATER_THAN_UINT16(12, aH);
  const uint16_t delta = tensH > onesH ? static_cast<uint16_t>(tensH - onesH)
                                       : static_cast<uint16_t>(onesH - tensH);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(4, delta);
}

void test_last_line_takes_leftover_height() {
  const DisplayLogic::Geometry geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x64, DisplayLogic::Orientation::Landscape);
  Core::DisplayContent lines[3] = {Core::DisplayContent::Clock, Core::DisplayContent::ClockAndEnd,
                                   Core::DisplayContent::Score};
  const DisplayLogic::LayoutPlan plan = DisplayLogic::planLayout(geometry, lines, 3);
  TEST_ASSERT_EQUAL_UINT16(21, plan.lines[0].height);
  TEST_ASSERT_EQUAL_UINT16(21, plan.lines[1].height);
  TEST_ASSERT_EQUAL_UINT16(22, plan.lines[2].height);
  TEST_ASSERT_EQUAL_UINT16(64, plan.lines[0].height + plan.lines[1].height + plan.lines[2].height);
}

void test_hero_line_is_larger_than_the_others() {
  const DisplayLogic::Geometry geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x64, DisplayLogic::Orientation::Landscape);
  Core::DisplayContent lines[4] = {Core::DisplayContent::Clock, Core::DisplayContent::ClockAndEnd,
                                   Core::DisplayContent::Score, Core::DisplayContent::ArrowCount};
  const DisplayLogic::LayoutPlan first =
      DisplayLogic::planLayout(geometry, lines, 3, DisplayLogic::LineScaleMode::Hero, 0);
  TEST_ASSERT_EQUAL_UINT16(32, first.lines[0].height);
  TEST_ASSERT_EQUAL_UINT16(16, first.lines[1].height);
  TEST_ASSERT_EQUAL_UINT16(16, first.lines[2].height);
  TEST_ASSERT_EQUAL_UINT16(64, first.lines[0].height + first.lines[1].height + first.lines[2].height);

  const DisplayLogic::LayoutPlan lastHero =
      DisplayLogic::planLayout(geometry, lines, 3, DisplayLogic::LineScaleMode::Hero, 2);
  TEST_ASSERT_EQUAL_UINT16(16, lastHero.lines[0].height);
  TEST_ASSERT_EQUAL_UINT16(16, lastHero.lines[1].height);
  TEST_ASSERT_EQUAL_UINT16(32, lastHero.lines[2].height);

  const DisplayLogic::LayoutPlan four =
      DisplayLogic::planLayout(geometry, lines, 4, DisplayLogic::LineScaleMode::Hero, 0);
  TEST_ASSERT_EQUAL_UINT16(28, four.lines[0].height);
  TEST_ASSERT_EQUAL_UINT16(12, four.lines[1].height);
  TEST_ASSERT_EQUAL_UINT16(12, four.lines[2].height);
  TEST_ASSERT_EQUAL_UINT16(12, four.lines[3].height);
  TEST_ASSERT_EQUAL_UINT16(64, four.lines[0].height + four.lines[1].height + four.lines[2].height +
                                   four.lines[3].height);
}

void test_ws2812_32x16_matches_the_original_map() {
  const DisplayLogic::Geometry geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::Led32x16, DisplayLogic::Orientation::Landscape);
  bool used[DisplayLogic::PIXEL_COUNT] = {};
  for (uint8_t y = 0; y < 16; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      const uint16_t index = DisplayLogic::ws2812Index(geometry, x, y);
      TEST_ASSERT_EQUAL_UINT16(DisplayLogic::ledIndex(x, y), index);
      TEST_ASSERT_FALSE(used[index]);
      used[index] = true;
    }
  }
}

void test_ws2812_64x32_uses_every_led_once() {
  const DisplayLogic::Geometry geometry =
      DisplayLogic::geometryFor(DisplayLogic::PanelPreset::P5_64x32, DisplayLogic::Orientation::Landscape);
  bool used[2048] = {};
  for (uint16_t y = 0; y < 32; y++) {
    for (uint16_t x = 0; x < 64; x++) {
      const uint16_t index = DisplayLogic::ws2812Index(geometry, x, y);
      TEST_ASSERT_LESS_THAN_UINT16(2048, index);
      TEST_ASSERT_FALSE(used[index]);
      used[index] = true;
    }
  }
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_named_sizes_and_portrait_swap);
  RUN_TEST(test_landscape_defaults_to_one_clock_line);
  RUN_TEST(test_64x32_clock_is_not_a_scaled_tile);
  RUN_TEST(test_portrait_stacks_more_lines_in_order);
  RUN_TEST(test_line_list_parsing);
  RUN_TEST(test_hub75_vertical_panels_are_rotated);
  RUN_TEST(test_hub75_three_vertical_uses_every_chain_pixel);
  RUN_TEST(test_smooth_glyphs_use_in_between_levels);
  RUN_TEST(test_wide_and_narrow_glyphs_share_one_scale);
  RUN_TEST(test_shooting_seconds_fill_the_large_panel);
  RUN_TEST(test_last_line_takes_leftover_height);
  RUN_TEST(test_hero_line_is_larger_than_the_others);
  RUN_TEST(test_ws2812_32x16_matches_the_original_map);
  RUN_TEST(test_ws2812_64x32_uses_every_led_once);
  return UNITY_END();
}
