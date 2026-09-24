#include <unity.h>

#include <cstring>

#include "browser/demo_api.h"
#include "core/display_logic.h"

void test_browser_host_runs_the_occupy_sequence() {
  demo_init(0);
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"IDLE\""));

  demo_tick(1000);
  TEST_ASSERT_EQUAL_INT(0, demo_control("start", 0));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"OCCUPY\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"light\":\"RED\""));

  demo_tick(1000 + 10000);
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"SHOOTING\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"light\":\"GREEN\""));
}

void test_browser_host_renders_a_logical_32x16_clock() {
  demo_init(0);
  TEST_ASSERT_EQUAL_UINT16(32, demo_panel_columns());
  TEST_ASSERT_EQUAL_UINT16(16, demo_panel_rows());

  demo_tick(0);
  const uint32_t* pixels = demo_logical_pixels();
  uint16_t lit = 0;
  for (uint16_t index = 0; index < 512; index++) {
    if (pixels[index] != 0) lit++;
  }
  TEST_ASSERT_GREATER_THAN_UINT16(20, lit);
  TEST_ASSERT_EQUAL_UINT32(pixels[0], demo_pixel(0));
}

void test_browser_host_refuses_an_unknown_control() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(1, demo_control("not-a-thing", 0));
}

void test_browser_host_defaults_to_abcd_rotation() {
  demo_init(0);
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdRotation\":true"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"details\":2"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"waves\":2"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"detail\":1"));
}

void test_browser_host_rotates_cd_first_on_the_second_end() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_session(
      "{\"abcdRotation\":true,\"details\":2,\"eventClass\":\"ANNOUNCED\",\"arrowsPerEnd\":3}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"detail\":1"));

  TEST_ASSERT_EQUAL_INT(0, demo_control("start", 0));
  demo_tick(10000);
  demo_tick(100000);
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"detail\":2"));
  demo_tick(110000);
  demo_tick(200000);
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"FINISHED\""));

  TEST_ASSERT_EQUAL_INT(0, demo_control("line_clear", 0));
  TEST_ASSERT_EQUAL_INT(0, demo_control("next_end", 0));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"end\":2"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"detail\":2"));
}

void test_browser_host_can_render_a_64x32_p5_layout() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_panel_options("{\"panelPreset\":\"P5_64X32\",\"orientation\":\"LANDSCAPE\"}"));
  TEST_ASSERT_EQUAL_UINT16(64, demo_panel_columns());
  TEST_ASSERT_EQUAL_UINT16(32, demo_panel_rows());
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"panelPreset\":\"P5_64X32\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"panelColumns\":64"));

  const uint32_t* pixels = demo_logical_pixels();
  uint16_t lit = 0;
  for (uint16_t index = 0; index < 2048; index++) {
    if (pixels[index] != 0) lit++;
  }
  TEST_ASSERT_GREATER_THAN_UINT16(80, lit);
}

void test_browser_host_can_render_stacked_vertical_p5s() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_panel_options("{\"panelPreset\":\"P5_96X64\",\"orientation\":\"LANDSCAPE\"}"));
  TEST_ASSERT_EQUAL_UINT16(96, demo_panel_columns());
  TEST_ASSERT_EQUAL_UINT16(64, demo_panel_rows());
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"panelPreset\":\"P5_96X64\""));
  TEST_ASSERT_EQUAL_INT(0, demo_panel_options(
      "{\"panelPreset\":\"P5_160X64\",\"orientation\":\"PORTRAIT\",\"lines\":\"CLOCK,SCORE,ARROWS\","
      "\"lineScale\":\"HERO\",\"heroLine\":0}"));
  TEST_ASSERT_EQUAL_UINT16(64, demo_panel_columns());
  TEST_ASSERT_EQUAL_UINT16(160, demo_panel_rows());
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"lineScale\":\"HERO\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"heroLine\":0"));
}

void test_browser_host_portrait_uses_the_extra_lines() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_panel_options(
      "{\"panelPreset\":\"P5_64X32\",\"orientation\":\"PORTRAIT\",\"lines\":\"CLOCK,CLOCK_END,SCORE\"}"));
  TEST_ASSERT_EQUAL_UINT16(32, demo_panel_columns());
  TEST_ASSERT_EQUAL_UINT16(64, demo_panel_rows());
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"panelLines\":\"CLOCK,CLOCK_END,SCORE\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"orientation\":\"PORTRAIT\""));
}

void test_browser_host_abcd_letters_default_white() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"abcdRotation\":true,\"details\":2}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdFollowTimer\":false"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdColour\":\"#ffffff\""));
  TEST_ASSERT_EQUAL_INT(0, demo_panel_options(
      "{\"showAbcd\":true,\"abcdVertical\":true,\"abcdFollowTimer\":false,\"abcdColour\":\"#ffffff\"}"));

  const uint32_t* pixels = demo_logical_pixels();
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_WHITE, pixels[2 * 32 + 1]);
}

void test_browser_host_reset_session_returns_to_end_one() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_control("start", 0));
  TEST_ASSERT_EQUAL_INT(0, demo_control("reset_session", 0));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"IDLE\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"end\":1"));
}

void test_browser_host_accepts_qualification_structure() {
  demo_init(0);
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"endsPerRound\":12"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"qualificationRounds\":2"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"round\":1"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"endInRound\":1"));

  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"endsPerRound\":10,\"qualificationRounds\":1}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"endsPerRound\":10"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"qualificationRounds\":1"));
}

void test_browser_host_accepts_one_two_and_three_waves() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"waves\":1}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"waves\":1"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"details\":1"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdRotation\":false"));

  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"waves\":2}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"waves\":2"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"details\":2"));

  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"waves\":3}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"waves\":3"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"details\":3"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdRotation\":true"));
}

void test_browser_host_accepts_break_length_in_minutes() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"breakMinutes\":20}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"breakMinutes\":20"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"breakSeconds\":1200"));
}

void test_browser_host_adjusts_upcoming_break_without_leaving_scoring() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_session(
      "{\"abcdRotation\":false,\"breakEnabled\":true,\"breakAfterEnds\":1,\"breakMinutes\":15,\"eventClass\":\"ANNOUNCED\",\"arrowsPerEnd\":3}"));
  TEST_ASSERT_EQUAL_INT(0, demo_control("start", 0));
  demo_tick(12000);
  TEST_ASSERT_EQUAL_INT(0, demo_control("stop", 0));
  TEST_ASSERT_EQUAL_INT(0, demo_control("line_clear", 0));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"SCORING\""));
  TEST_ASSERT_EQUAL_INT(0, demo_control("adjust_break", 60));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"phase\":\"SCORING\""));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"breakMinutes\":16"));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_browser_host_runs_the_occupy_sequence);
  RUN_TEST(test_browser_host_renders_a_logical_32x16_clock);
  RUN_TEST(test_browser_host_refuses_an_unknown_control);
  RUN_TEST(test_browser_host_defaults_to_abcd_rotation);
  RUN_TEST(test_browser_host_rotates_cd_first_on_the_second_end);
  RUN_TEST(test_browser_host_can_render_a_64x32_p5_layout);
  RUN_TEST(test_browser_host_can_render_stacked_vertical_p5s);
  RUN_TEST(test_browser_host_portrait_uses_the_extra_lines);
  RUN_TEST(test_browser_host_abcd_letters_default_white);
  RUN_TEST(test_browser_host_reset_session_returns_to_end_one);
  RUN_TEST(test_browser_host_accepts_one_two_and_three_waves);
  RUN_TEST(test_browser_host_accepts_break_length_in_minutes);
  RUN_TEST(test_browser_host_adjusts_upcoming_break_without_leaving_scoring);
  RUN_TEST(test_browser_host_accepts_qualification_structure);
  return UNITY_END();
}
