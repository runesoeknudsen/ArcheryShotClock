#include <unity.h>

#include <cstring>

#include "core/display_logic.h"

namespace {

uint32_t frame[DisplayLogic::PIXEL_COUNT];

DisplayLogic::RenderResult renderClock(uint32_t remainingMs, Core::Light light) {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = light;
  request.remainingMs = remainingMs;
  return DisplayLogic::renderFrame(request, frame);
}

uint32_t pixelAt(uint8_t x, uint8_t y) { return frame[DisplayLogic::ledIndex(x, y)]; }

}  // namespace

void test_led_mapping_uses_every_pixel_once() {
  bool used[DisplayLogic::PIXEL_COUNT] = {};

  for (uint8_t y = 0; y < DisplayLogic::ROWS; y++) {
    for (uint8_t x = 0; x < DisplayLogic::COLUMNS; x++) {
      const uint16_t index = DisplayLogic::ledIndex(x, y);
      TEST_ASSERT_LESS_THAN_UINT16(DisplayLogic::PIXEL_COUNT, index);
      TEST_ASSERT_FALSE(used[index]);
      used[index] = true;
    }
  }

  for (uint16_t index = 0; index < DisplayLogic::PIXEL_COUNT; index++) TEST_ASSERT_TRUE(used[index]);
}

void test_idle_cannot_be_mistaken_for_the_warning_colour() {
  // The two are read at distance by people who have been trained that yellow
  // means thirty seconds, so they must not be neighbours in hue.
  const uint32_t idle = DisplayLogic::colourFor(Core::Light::Off);
  const uint32_t yellow = DisplayLogic::colourFor(Core::Light::Yellow);
  TEST_ASSERT_NOT_EQUAL_UINT32(yellow, idle);

  // Blue is what separates them: the warning has none, idle has plenty.
  const uint8_t idleBlue = idle & 0xFF;
  const uint8_t yellowBlue = yellow & 0xFF;
  TEST_ASSERT_GREATER_THAN_UINT8(0x80, idleBlue);
  TEST_ASSERT_LESS_THAN_UINT8(0x40, yellowBlue);

  // And idle is not one of the three signal colours by accident.
  TEST_ASSERT_NOT_EQUAL_UINT32(DisplayLogic::COLOUR_RED, idle);
  TEST_ASSERT_NOT_EQUAL_UINT32(DisplayLogic::COLOUR_GREEN, idle);
  TEST_ASSERT_NOT_EQUAL_UINT32(DisplayLogic::COLOUR_YELLOW, idle);
}

void test_art_11_3_1_colour_follows_the_light_state() {
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_RED, DisplayLogic::colourFor(Core::Light::Red));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, DisplayLogic::colourFor(Core::Light::Green));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_YELLOW, DisplayLogic::colourFor(Core::Light::Yellow));

  renderClock(60000, Core::Light::Green);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(15, 5));

  renderClock(60000, Core::Light::Yellow);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_YELLOW, pixelAt(15, 5));

  renderClock(0, Core::Light::Red);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_RED, pixelAt(15, 5));
}

void test_clock_shows_minutes_and_seconds() {
  DisplayLogic::RenderResult result = renderClock(0, Core::Light::Red);
  TEST_ASSERT_EQUAL_STRING("00:00", result.text);

  result = renderClock(90000, Core::Light::Green);
  TEST_ASSERT_EQUAL_STRING("01:30", result.text);

  result = renderClock(20000, Core::Light::Green);
  TEST_ASSERT_EQUAL_STRING("00:20", result.text);
}

void test_clock_can_show_seconds_only() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Green;
  request.clockSeconds = true;

  request.remainingMs = 0;
  TEST_ASSERT_EQUAL_STRING("0", DisplayLogic::renderFrame(request, frame).text);

  request.remainingMs = 20000;
  TEST_ASSERT_EQUAL_STRING("20", DisplayLogic::renderFrame(request, frame).text);

  request.remainingMs = 90000;
  TEST_ASSERT_EQUAL_STRING("90", DisplayLogic::renderFrame(request, frame).text);

  request.remainingMs = 120000;
  TEST_ASSERT_EQUAL_STRING("120", DisplayLogic::renderFrame(request, frame).text);
}

void test_clock_rounds_up_so_zero_means_time_is_over() {
  // Any part of a second left still reads as that second, and the panel only
  // shows 00:00 when the period has genuinely expired.
  TEST_ASSERT_EQUAL_STRING("00:01", renderClock(1, Core::Light::Green).text);
  TEST_ASSERT_EQUAL_STRING("00:01", renderClock(999, Core::Light::Green).text);
  TEST_ASSERT_EQUAL_STRING("00:01", renderClock(1000, Core::Light::Green).text);
  TEST_ASSERT_EQUAL_STRING("00:02", renderClock(1001, Core::Light::Green).text);
  TEST_ASSERT_EQUAL_STRING("00:00", renderClock(0, Core::Light::Red).text);
}

void test_selected_content_changes_what_is_drawn() {
  DisplayLogic::RenderRequest request;
  request.light = Core::Light::Green;
  request.remainingMs = 90000;
  request.endNumber = 4;
  request.arrowsShot = 2;
  request.arrowsPerEnd = 3;

  request.content = Core::DisplayContent::ClockAndEnd;
  request.showEndLabels = false;
  TEST_ASSERT_EQUAL_STRING("04", DisplayLogic::renderFrame(request, frame).text);

  request.content = Core::DisplayContent::ArrowCount;
  TEST_ASSERT_EQUAL_STRING("2/3", DisplayLogic::renderFrame(request, frame).text);

  request.content = Core::DisplayContent::Clock;
  TEST_ASSERT_EQUAL_STRING("01:30", DisplayLogic::renderFrame(request, frame).text);
}

void test_blank_content_lights_nothing() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Blank;
  request.light = Core::Light::Red;
  request.remainingMs = 90000;

  const DisplayLogic::RenderResult result = DisplayLogic::renderFrame(request, frame);

  TEST_ASSERT_EQUAL_UINT16(0, result.litPixels);
  TEST_ASSERT_EQUAL_STRING("", result.text);
  for (uint16_t index = 0; index < DisplayLogic::PIXEL_COUNT; index++) TEST_ASSERT_EQUAL_UINT32(0, frame[index]);
}

void test_every_content_now_has_data_behind_it() {
  // Score, set points and the shooter marker became available with the match
  // module; before that they were refused rather than shown empty.
  TEST_ASSERT_TRUE(DisplayLogic::contentAvailable(Core::DisplayContent::Clock));
  TEST_ASSERT_TRUE(DisplayLogic::contentAvailable(Core::DisplayContent::ArrowCount));
  TEST_ASSERT_TRUE(DisplayLogic::contentAvailable(Core::DisplayContent::Score));
  TEST_ASSERT_TRUE(DisplayLogic::contentAvailable(Core::DisplayContent::SetPoints));
  TEST_ASSERT_TRUE(DisplayLogic::contentAvailable(Core::DisplayContent::Shooter));
}

void test_score_and_set_points_show_both_sides() {
  DisplayLogic::RenderRequest request;
  request.light = Core::Light::Red;
  request.score[0] = 87;
  request.score[1] = 84;
  request.setPoints[0] = 4;
  request.setPoints[1] = 2;
  request.shooter = 2;

  request.content = Core::DisplayContent::Score;
  TEST_ASSERT_EQUAL_STRING("87-84", DisplayLogic::renderFrame(request, frame).text);

  request.content = Core::DisplayContent::SetPoints;
  TEST_ASSERT_EQUAL_STRING("04-02", DisplayLogic::renderFrame(request, frame).text);

  request.content = Core::DisplayContent::Shooter;
  TEST_ASSERT_EQUAL_STRING("02", DisplayLogic::renderFrame(request, frame).text);
}

void test_scores_beyond_two_digits_are_clamped_rather_than_wrapped() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Score;
  request.light = Core::Light::Red;
  request.score[0] = 145;
  request.score[1] = 9;

  // A three-digit running total will not fit; showing 99 is wrong but visibly
  // wrong, where a wrapped 45 would look plausible and mislead.
  TEST_ASSERT_EQUAL_STRING("99-09", DisplayLogic::renderFrame(request, frame).text);
}

void test_checksum_distinguishes_frames_and_repeats_for_identical_ones() {
  const uint32_t first = renderClock(90000, Core::Light::Green).checksum;
  const uint32_t same = renderClock(90000, Core::Light::Green).checksum;
  const uint32_t laterTime = renderClock(89000, Core::Light::Green).checksum;
  const uint32_t otherColour = renderClock(90000, Core::Light::Yellow).checksum;

  TEST_ASSERT_EQUAL_UINT32(first, same);
  TEST_ASSERT_NOT_EQUAL_UINT32(first, laterTime);
  TEST_ASSERT_NOT_EQUAL_UINT32(first, otherColour);
}

void test_clock_seconds_are_right_aligned() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Green;
  request.clockSeconds = true;

  request.remainingMs = 0;
  DisplayLogic::renderFrame(request, frame);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(26, 2));

  request.remainingMs = 20000;
  DisplayLogic::renderFrame(request, frame);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(26, 2));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(19, 2));
}

void test_abcd_sits_with_the_seconds_clock() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Green;
  request.clockSeconds = true;
  request.remainingMs = 20000;
  request.details = 2;
  request.detail = 1;
  request.showAbcd = true;
  request.abcdVertical = true;

  DisplayLogic::RenderResult result = DisplayLogic::renderFrame(request, frame);
  TEST_ASSERT_EQUAL_STRING("AB 20", result.text);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_WHITE, pixelAt(2, 2));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(26, 2));

  request.abcdVertical = false;
  result = DisplayLogic::renderFrame(request, frame);
  TEST_ASSERT_EQUAL_STRING("AB 20", result.text);
  TEST_ASSERT_EQUAL_UINT32(0, pixelAt(2, 2));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_WHITE, pixelAt(24, 11));

  request.detail = 2;
  request.abcdVertical = true;
  TEST_ASSERT_EQUAL_STRING("CD 20", DisplayLogic::renderFrame(request, frame).text);
}

void test_abcd_letters_can_follow_the_timer_or_use_a_fixed_colour() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Green;
  request.clockSeconds = true;
  request.remainingMs = 20000;
  request.details = 2;
  request.detail = 1;
  request.showAbcd = true;
  request.abcdVertical = true;

  request.abcdFollowTimer = true;
  DisplayLogic::renderFrame(request, frame);
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(2, 2));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(26, 2));

  request.abcdFollowTimer = false;
  request.abcdColour = 0x3366CCu;
  DisplayLogic::renderFrame(request, frame);
  TEST_ASSERT_EQUAL_HEX32(0x3366CCu, pixelAt(2, 2));
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_GREEN, pixelAt(26, 2));
}

void test_css_colour_round_trips() {
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFu, DisplayLogic::parseCssColour("#ffffff", 0));
  TEST_ASSERT_EQUAL_HEX32(0x3366CCu, DisplayLogic::parseCssColour("3366cc", 0));
  TEST_ASSERT_EQUAL_HEX32(0x112233u, DisplayLogic::parseCssColour("not-a-colour", 0x112233u));

  char formatted[8] = {};
  DisplayLogic::formatCssColour(0x3366CCu, formatted, sizeof(formatted));
  TEST_ASSERT_EQUAL_STRING("#3366cc", formatted);
}

void test_finished_and_scoring_show_the_end_that_just_ran() {
  DisplayLogic::RenderRequest request;
  request.content = Core::DisplayContent::Clock;
  request.light = Core::Light::Red;
  request.clockSeconds = true;
  request.remainingMs = 0;
  request.endNumber = 12;
  request.showEndLabels = true;

  request.phase = Core::Phase::Finished;
  TEST_ASSERT_EQUAL_STRING("End 12", DisplayLogic::renderFrame(request, frame).text);

  request.phase = Core::Phase::Scoring;
  TEST_ASSERT_EQUAL_STRING("Scoring 12", DisplayLogic::renderFrame(request, frame).text);

  request.showEndLabels = false;
  request.phase = Core::Phase::Finished;
  TEST_ASSERT_EQUAL_STRING("0", DisplayLogic::renderFrame(request, frame).text);
}

void test_frame_clears_pixels_not_used_by_digits() {
  renderClock(0, Core::Light::Red);

  TEST_ASSERT_EQUAL_UINT32(0, pixelAt(0, 0));
  TEST_ASSERT_EQUAL_UINT32(0, pixelAt(31, 15));
  TEST_ASSERT_EQUAL_UINT32(0, pixelAt(7, 8));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_led_mapping_uses_every_pixel_once);
  RUN_TEST(test_art_11_3_1_colour_follows_the_light_state);
  RUN_TEST(test_idle_cannot_be_mistaken_for_the_warning_colour);
  RUN_TEST(test_clock_shows_minutes_and_seconds);
  RUN_TEST(test_clock_can_show_seconds_only);
  RUN_TEST(test_clock_seconds_are_right_aligned);
  RUN_TEST(test_abcd_sits_with_the_seconds_clock);
  RUN_TEST(test_abcd_letters_can_follow_the_timer_or_use_a_fixed_colour);
  RUN_TEST(test_css_colour_round_trips);
  RUN_TEST(test_finished_and_scoring_show_the_end_that_just_ran);
  RUN_TEST(test_clock_rounds_up_so_zero_means_time_is_over);
  RUN_TEST(test_selected_content_changes_what_is_drawn);
  RUN_TEST(test_blank_content_lights_nothing);
  RUN_TEST(test_every_content_now_has_data_behind_it);
  RUN_TEST(test_score_and_set_points_show_both_sides);
  RUN_TEST(test_scores_beyond_two_digits_are_clamped_rather_than_wrapped);
  RUN_TEST(test_checksum_distinguishes_frames_and_repeats_for_identical_ones);
  RUN_TEST(test_frame_clears_pixels_not_used_by_digits);
  return UNITY_END();
}
