#include <unity.h>

#include "core/button_logic.h"

namespace {
// Feeds a steady level for a span of milliseconds, one millisecond at a time,
// and reports the last event seen.
Core::ButtonEvent hold(Core::ButtonDebouncer& button, uint32_t& now, bool pressed, uint32_t durationMs) {
  Core::ButtonEvent last = Core::ButtonEvent::None;
  for (uint32_t elapsed = 0; elapsed < durationMs; elapsed++) {
    const Core::ButtonEvent event = button.update(now, pressed);
    if (event != Core::ButtonEvent::None) last = event;
    now++;
  }
  return last;
}
}  // namespace

void test_a_press_is_reported_once_the_level_has_settled() {
  Core::ButtonDebouncer button(25, 800);
  uint32_t now = 1000;

  // Nothing while the level is still younger than the debounce window.
  TEST_ASSERT_EQUAL(Core::ButtonEvent::None, hold(button, now, true, 20));
  TEST_ASSERT_FALSE(button.isPressed());

  TEST_ASSERT_EQUAL(Core::ButtonEvent::Press, hold(button, now, true, 10));
  TEST_ASSERT_TRUE(button.isPressed());
}

void test_contact_bounce_produces_exactly_one_press() {
  Core::ButtonDebouncer button(25, 800);
  uint32_t now = 1000;
  uint8_t presses = 0;

  // A switch chattering for 15 ms before settling closed.
  for (uint8_t cycle = 0; cycle < 5; cycle++) {
    if (button.update(now, cycle % 2 == 0) == Core::ButtonEvent::Press) presses++;
    now += 3;
  }
  for (uint32_t elapsed = 0; elapsed < 60; elapsed++) {
    if (button.update(now, true) == Core::ButtonEvent::Press) presses++;
    now++;
  }

  TEST_ASSERT_EQUAL_UINT8(1, presses);
}

void test_a_long_press_is_reported_while_the_button_is_still_down() {
  Core::ButtonDebouncer button(25, 800);
  uint32_t now = 1000;
  hold(button, now, true, 30);

  TEST_ASSERT_EQUAL(Core::ButtonEvent::None, hold(button, now, true, 700));
  TEST_ASSERT_EQUAL(Core::ButtonEvent::LongPress, hold(button, now, true, 120));
  // And only once, however long it is held after that.
  TEST_ASSERT_EQUAL(Core::ButtonEvent::None, hold(button, now, true, 3000));
}

void test_a_short_press_never_reports_a_long_press() {
  Core::ButtonDebouncer button(25, 800);
  uint32_t now = 1000;

  hold(button, now, true, 200);
  TEST_ASSERT_EQUAL(Core::ButtonEvent::Release, hold(button, now, false, 40));

  // Holding again starts the long-press clock over rather than accumulating.
  hold(button, now, true, 30);
  TEST_ASSERT_EQUAL(Core::ButtonEvent::None, hold(button, now, true, 700));
}

void test_release_is_reported_and_the_button_reads_as_up() {
  Core::ButtonDebouncer button(25, 800);
  uint32_t now = 1000;
  hold(button, now, true, 100);

  TEST_ASSERT_EQUAL(Core::ButtonEvent::Release, hold(button, now, false, 40));
  TEST_ASSERT_FALSE(button.isPressed());
  TEST_ASSERT_EQUAL_UINT32(0, button.heldMs(now));
}

void test_hold_duration_is_reported_for_the_caller() {
  Core::ButtonDebouncer button(25, 800);
  uint32_t now = 1000;

  // The press settles 25 ms after the level went high, and the hold is
  // measured from that moment, not from the first bounce.
  hold(button, now, true, 26);
  const uint32_t settledAt = now - 1;

  hold(button, now, true, 500);

  TEST_ASSERT_EQUAL_UINT32(now - settledAt, button.heldMs(now));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_press_is_reported_once_the_level_has_settled);
  RUN_TEST(test_contact_bounce_produces_exactly_one_press);
  RUN_TEST(test_a_long_press_is_reported_while_the_button_is_still_down);
  RUN_TEST(test_a_short_press_never_reports_a_long_press);
  RUN_TEST(test_release_is_reported_and_the_button_reads_as_up);
  RUN_TEST(test_hold_duration_is_reported_for_the_caller);
  return UNITY_END();
}
