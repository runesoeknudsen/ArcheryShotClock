#include <unity.h>

#include "core/sound.h"

// Article 11.3 fixes how many sound signals each meaning gets and says nothing
// else about them, so these tests check counts and separation, never tone.

class FakeSoundOutput : public Core::SoundOutput {
public:
  void setActive(bool active) override {
    if (stateCount < sizeof(states) / sizeof(states[0])) states[stateCount++] = active;
    if (active) risingEdges++;
  }

  bool states[32] = {};
  uint8_t stateCount = 0;
  uint8_t risingEdges = 0;
};

namespace {
// Runs the controller forward the way the main loop would, so a beep is never
// skipped by a single large jump in time.
void runFor(Core::SoundController& sound, uint32_t from, uint32_t durationMs, uint32_t stepMs = 10) {
  for (uint32_t elapsed = 0; elapsed <= durationMs; elapsed += stepMs) sound.update(from + elapsed);
}
}  // namespace

void test_each_article_11_3_signal_sounds_its_prescribed_number_of_beeps() {
  const Core::SignalCode codes[] = {Core::SignalCode::OccupyLine, Core::SignalCode::Start, Core::SignalCode::Stop,
                                    Core::SignalCode::Scoring, Core::SignalCode::Resume};
  const uint8_t expected[] = {2, 1, 2, 3, 1};

  for (uint8_t index = 0; index < 5; index++) {
    FakeSoundOutput output;
    Core::SoundController sound(output);

    sound.playSignal(codes[index], 0);
    runFor(sound, 0, 5000);

    TEST_ASSERT_EQUAL_UINT8(expected[index], Core::signalCount(codes[index]));
    TEST_ASSERT_EQUAL_UINT8(expected[index], output.risingEdges);
    TEST_ASSERT_FALSE(sound.isPlaying());
    TEST_ASSERT_FALSE(output.states[output.stateCount - 1]);
  }
}

void test_art_11_3_3_emergency_sounds_at_least_five_beeps() {
  FakeSoundOutput output;
  Core::SoundController sound(output);

  sound.playSignal(Core::SignalCode::Emergency, 0);
  runFor(sound, 0, 8000);

  TEST_ASSERT_GREATER_OR_EQUAL_UINT8(5, output.risingEdges);
}

void test_beeps_are_separated_so_two_are_never_heard_as_one() {
  FakeSoundOutput output;
  Core::SoundController sound(output);
  sound.setPattern(100, 80);

  sound.playSignal(Core::SignalCode::Stop, 1000);
  // First beep is on for its full length and no more.
  runFor(sound, 1000, 90);
  TEST_ASSERT_EQUAL_UINT8(1, output.stateCount);
  TEST_ASSERT_TRUE(output.states[0]);

  sound.update(1100);
  TEST_ASSERT_EQUAL_UINT8(2, output.stateCount);
  TEST_ASSERT_FALSE(output.states[1]);

  // Silence lasts the whole gap before the second beep begins.
  sound.update(1179);
  TEST_ASSERT_EQUAL_UINT8(2, output.stateCount);
  sound.update(1180);
  TEST_ASSERT_TRUE(output.states[2]);

  runFor(sound, 1180, 200);
  TEST_ASSERT_EQUAL_UINT8(2, output.risingEdges);
  TEST_ASSERT_FALSE(sound.isPlaying());
}

void test_a_signal_plays_once_and_does_not_repeat() {
  FakeSoundOutput output;
  Core::SoundController sound(output);

  sound.playSignal(Core::SignalCode::Scoring, 0);
  runFor(sound, 0, 5000);
  const uint8_t after = output.stateCount;

  runFor(sound, 5000, 20000);
  TEST_ASSERT_EQUAL_UINT8(after, output.stateCount);
}

void test_disabling_sound_silences_a_signal_in_progress() {
  FakeSoundOutput output;
  Core::SoundController sound(output);

  sound.playSignal(Core::SignalCode::Scoring, 100);
  sound.setEnabled(false);
  runFor(sound, 100, 5000);

  TEST_ASSERT_FALSE(sound.isPlaying());
  TEST_ASSERT_FALSE(output.states[output.stateCount - 1]);
  TEST_ASSERT_EQUAL_UINT8(1, output.risingEdges);
}

void test_pattern_length_is_configurable_because_the_rules_do_not_fix_it() {
  FakeSoundOutput output;
  Core::SoundController sound(output);

  sound.setPattern(400, 300);
  TEST_ASSERT_EQUAL_UINT16(400, sound.beepMs());
  TEST_ASSERT_EQUAL_UINT16(300, sound.gapMs());

  // Zero is refused rather than producing a signal nobody can hear or count.
  sound.setPattern(0, 0);
  TEST_ASSERT_EQUAL_UINT16(400, sound.beepMs());
  TEST_ASSERT_EQUAL_UINT16(300, sound.gapMs());
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_each_article_11_3_signal_sounds_its_prescribed_number_of_beeps);
  RUN_TEST(test_art_11_3_3_emergency_sounds_at_least_five_beeps);
  RUN_TEST(test_beeps_are_separated_so_two_are_never_heard_as_one);
  RUN_TEST(test_a_signal_plays_once_and_does_not_repeat);
  RUN_TEST(test_disabling_sound_silences_a_signal_in_progress);
  RUN_TEST(test_pattern_length_is_configurable_because_the_rules_do_not_fix_it);
  return UNITY_END();
}
