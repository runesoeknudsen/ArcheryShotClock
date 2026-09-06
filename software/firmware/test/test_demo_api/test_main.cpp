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

void test_browser_host_abcd_letters_default_white() {
  demo_init(0);
  TEST_ASSERT_EQUAL_INT(0, demo_session("{\"abcdRotation\":true,\"details\":2}"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdFollowTimer\":false"));
  TEST_ASSERT_NOT_NULL(strstr(demo_state_json(), "\"abcdColour\":\"#ffffff\""));
  TEST_ASSERT_EQUAL_INT(0, demo_panel_options(
      "{\"showAbcd\":true,\"abcdVertical\":true,\"abcdFollowTimer\":false,\"abcdColour\":\"#ffffff\"}"));

  const uint32_t* pixels = demo_logical_pixels();
  TEST_ASSERT_EQUAL_HEX32(DisplayLogic::COLOUR_WHITE, pixels[2 * 32 + 2]);
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
  RUN_TEST(test_browser_host_abcd_letters_default_white);
  return UNITY_END();
}
