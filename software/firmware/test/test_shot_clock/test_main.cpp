#include <unity.h>

#include <string>
#include <vector>

#include "core/shot_clock.h"

// Tests are named after the article they check, so a failure points straight at
// the rule that was broken.

namespace {

class RecordingSink : public Core::TraceSink {
public:
  void write(const char* line, uint16_t length) override { lines.push_back(std::string(line, length)); }

  bool contains(const char* needle) const {
    for (const std::string& line : lines) {
      if (line.find(needle) != std::string::npos) return true;
    }
    return false;
  }

  std::vector<std::string> lines;
};

struct Harness {
  RecordingSink sink;
  Core::Tracer tracer{sink};

  // Recording is off by default on the device; these tests are testing mode.
  Harness() { tracer.setLevel(Core::TraceLevel::Normal); }

  Core::ShotClock clock{tracer};
  uint32_t now = 1000;

  void advance(uint32_t ms) {
    now += ms;
    clock.update(now);
  }

  // Advances in one-second steps so phase transitions are observed the way the
  // main loop would see them, not skipped over in a single jump.
  void advanceSeconds(uint32_t seconds) {
    for (uint32_t index = 0; index < seconds; index++) advance(1000);
  }

  std::vector<Core::SignalCode> drainSignals() {
    std::vector<Core::SignalCode> collected;
    Core::SignalCode signal = Core::SignalCode::None;
    while (clock.takeSignal(signal)) collected.push_back(signal);
    return collected;
  }

  bool logContains(const char* needle) const { return sink.contains(needle); }

  void configure(Core::Mode mode, Rules::EventClass eventClass, uint8_t arrows) {
    Core::SessionConfig config;
    config.mode = mode;
    config.eventClass = eventClass;
    config.arrowsPerEnd = arrows;
    // Sequence tests are one-detail unless they are testing Art. 11.2.3.1.
    config.abcdRotation = false;
    clock.configure(now, config);
  }
};

}  // namespace

void test_art_11_2_1_1_announced_events_allow_30_s_per_arrow() {
  TEST_ASSERT_EQUAL_UINT32(
      30000, Rules::perArrowMs(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced));
}

void test_art_11_2_1_2_other_events_allow_40_s_reducible_to_30() {
  TEST_ASSERT_EQUAL_UINT32(40000, Rules::perArrowMs(Core::Mode::IndividualNonAlternating, Rules::EventClass::Other));
  TEST_ASSERT_EQUAL_UINT32(
      30000, Rules::perArrowMs(Core::Mode::IndividualNonAlternating, Rules::EventClass::OtherReduced));
}

void test_art_11_2_1_alternating_and_team_are_20_s_in_both_classes() {
  TEST_ASSERT_EQUAL_UINT32(20000, Rules::perArrowMs(Core::Mode::IndividualAlternating, Rules::EventClass::Announced));
  TEST_ASSERT_EQUAL_UINT32(20000, Rules::perArrowMs(Core::Mode::IndividualAlternating, Rules::EventClass::Other));
  TEST_ASSERT_EQUAL_UINT32(20000, Rules::perArrowMs(Core::Mode::TeamSimultaneous, Rules::EventClass::Announced));
  TEST_ASSERT_EQUAL_UINT32(20000, Rules::perArrowMs(Core::Mode::MixedTeam, Rules::EventClass::Other));
}

void test_art_11_2_1_period_is_arrows_times_per_arrow() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Other, 3);
  TEST_ASSERT_EQUAL_UINT32(120000, harness.clock.snapshot().periodMs);

  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 6);
  TEST_ASSERT_EQUAL_UINT32(180000, harness.clock.snapshot().periodMs);
}

void test_art_11_3_1_full_sequence_of_lights_and_signals() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.drainSignals();

  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Red, harness.clock.snapshot().light);
  std::vector<Core::SignalCode> signals = harness.drainSignals();
  TEST_ASSERT_EQUAL_UINT32(1, signals.size());
  TEST_ASSERT_EQUAL(Core::SignalCode::OccupyLine, signals[0]);
  TEST_ASSERT_EQUAL_UINT8(2, Core::signalCount(Core::SignalCode::OccupyLine));

  // Green ten seconds later, one sound signal.
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Green, harness.clock.snapshot().light);
  TEST_ASSERT_EQUAL_UINT32(90000, harness.clock.snapshot().remainingMs);
  signals = harness.drainSignals();
  TEST_ASSERT_EQUAL_UINT32(1, signals.size());
  TEST_ASSERT_EQUAL(Core::SignalCode::Start, signals[0]);
  TEST_ASSERT_EQUAL_UINT8(1, Core::signalCount(Core::SignalCode::Start));

  // Yellow thirty seconds before the end, with no sound signal of its own.
  harness.advanceSeconds(60);
  TEST_ASSERT_EQUAL(Core::Phase::Warning, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Yellow, harness.clock.snapshot().light);
  TEST_ASSERT_EQUAL_UINT32(30000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_EQUAL_UINT32(0, harness.drainSignals().size());

  // Red and two sound signals when the time is finished.
  harness.advanceSeconds(30);
  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Red, harness.clock.snapshot().light);
  TEST_ASSERT_EQUAL_UINT32(0, harness.clock.snapshot().remainingMs);
  signals = harness.drainSignals();
  TEST_ASSERT_EQUAL_UINT32(1, signals.size());
  TEST_ASSERT_EQUAL(Core::SignalCode::Stop, signals[0]);
  TEST_ASSERT_EQUAL_UINT8(2, Core::signalCount(Core::SignalCode::Stop));

  // Three sound signals for scoring once the line is clear.
  harness.clock.lineClear(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Scoring, harness.clock.snapshot().phase);
  signals = harness.drainSignals();
  TEST_ASSERT_EQUAL_UINT32(1, signals.size());
  TEST_ASSERT_EQUAL(Core::SignalCode::Scoring, signals[0]);
  TEST_ASSERT_EQUAL_UINT8(3, Core::signalCount(Core::SignalCode::Scoring));
}

void test_art_11_3_1_stop_finishes_even_with_arrows_unshot() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.advanceSeconds(5);
  harness.drainSignals();

  harness.clock.stop(harness.now);

  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(0, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_EQUAL(Core::SignalCode::Stop, harness.drainSignals()[0]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"what\":\"stop\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"reason\":\"director\""));
}

void test_art_11_3_1_no_yellow_warning_in_alternating_shooting() {
  Harness harness;
  harness.configure(Core::Mode::IndividualAlternating, Rules::EventClass::Announced, 1);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);

  // The whole period is 20 s, well inside the 30 s warning threshold.
  harness.advanceSeconds(5);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Green, harness.clock.snapshot().light);
}

void test_art_11_2_4_1_individual_resume_is_a_flat_per_arrow_allowance() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  Core::SessionConfig config = harness.clock.config();
  config.replayOccupyOnResume = false;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.clock.addArrow(harness.now);
  harness.advanceSeconds(80);  // 10 s left on a 90 s period, two arrows unshot
  TEST_ASSERT_EQUAL_UINT32(10000, harness.clock.snapshot().remainingMs);

  harness.clock.suspend(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Suspended, harness.clock.snapshot().phase);
  harness.drainSignals();

  harness.clock.resume(harness.now);

  // Two unshot arrows at 30 s each, regardless of the 10 s left on the clock.
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(60000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.2.4.1\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"result_ms\":60000"));
  TEST_ASSERT_EQUAL(Core::SignalCode::Resume, harness.drainSignals()[0]);
}

void test_art_11_2_4_2_team_resume_keeps_a_clock_that_beats_the_floor() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::TeamSimultaneous;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 6;
  config.abcdRotation = false;
  config.replayOccupyOnResume = false;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.clock.addArrow(harness.now);
  harness.clock.addArrow(harness.now);
  // 120 s period, 20 s elapsed: 100 s on the clock, four arrows unshot, so the
  // 80 s floor is beaten and the clock is kept.
  harness.advanceSeconds(20);
  TEST_ASSERT_EQUAL_UINT32(100000, harness.clock.snapshot().remainingMs);

  harness.clock.suspend(harness.now);
  harness.clock.resume(harness.now);

  TEST_ASSERT_EQUAL_UINT32(100000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.2.4.2\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"reason\":\"clock>floor\""));
}

void test_art_11_2_4_2_team_resume_lifts_a_clock_below_the_floor() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::TeamSimultaneous;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 6;
  config.abcdRotation = false;
  config.replayOccupyOnResume = false;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.clock.addArrow(harness.now);
  harness.clock.addArrow(harness.now);
  harness.clock.addArrow(harness.now);
  // 120 s period, 90 s elapsed: 30 s left, three arrows unshot, floor is 60 s.
  harness.advanceSeconds(90);
  TEST_ASSERT_EQUAL_UINT32(30000, harness.clock.snapshot().remainingMs);

  harness.clock.suspend(harness.now);
  harness.clock.resume(harness.now);

  TEST_ASSERT_EQUAL_UINT32(60000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_TRUE(harness.sink.contains("\"reason\":\"clock<=floor\""));
}

void test_art_11_2_4_resume_replays_the_ten_second_period_by_default() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.advanceSeconds(10);
  harness.clock.suspend(harness.now);
  harness.drainSignals();

  harness.clock.resume(harness.now);

  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(10000, harness.clock.snapshot().remainingMs);
  std::vector<Core::SignalCode> signals = harness.drainSignals();
  TEST_ASSERT_EQUAL(Core::SignalCode::Resume, signals[0]);
  TEST_ASSERT_EQUAL(Core::SignalCode::OccupyLine, signals[1]);

  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(90000, harness.clock.snapshot().remainingMs);
}

void test_art_11_2_2_time_may_be_extended() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.advanceSeconds(60);
  TEST_ASSERT_EQUAL_UINT32(30000, harness.clock.snapshot().remainingMs);

  harness.clock.extendTime(harness.now, 15000);

  TEST_ASSERT_EQUAL_UINT32(45000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.2.2\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"added_ms\":15000"));
}

void test_art_11_3_3_emergency_gives_at_least_five_signals_from_any_phase() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.clock.start(harness.now);
  harness.advanceSeconds(12);
  harness.drainSignals();

  harness.clock.emergency(harness.now);

  TEST_ASSERT_EQUAL(Core::Phase::Emergency, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Red, harness.clock.snapshot().light);
  TEST_ASSERT_FALSE(harness.clock.snapshot().running);
  TEST_ASSERT_EQUAL(Core::SignalCode::Emergency, harness.drainSignals()[0]);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT8(5, Core::signalCount(Core::SignalCode::Emergency));
}

void test_art_10_1_next_end_advances_and_clears_the_arrow_count() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.clock.addArrow(harness.now);
  harness.clock.addArrow(harness.now);
  harness.clock.stop(harness.now);
  harness.clock.lineClear(harness.now);

  harness.clock.nextEnd(harness.now);

  TEST_ASSERT_EQUAL_UINT16(2, harness.clock.snapshot().endNumber);
  TEST_ASSERT_EQUAL_UINT8(0, harness.clock.snapshot().arrowsShot);
  TEST_ASSERT_EQUAL(Core::Phase::Idle, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(90000, harness.clock.snapshot().remainingMs);
}

void test_a_break_follows_scoring_after_the_configured_number_of_ends() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  config.breakEnabled = true;
  config.breakAfterEnds = 1;
  config.breakMs = 30000;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(12);
  harness.clock.stop(harness.now);
  harness.clock.lineClear(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Scoring, harness.clock.snapshot().phase);

  harness.clock.nextEnd(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Break, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT16(1, harness.clock.snapshot().endNumber);
  TEST_ASSERT_EQUAL_UINT32(30000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_EQUAL(Core::Light::Off, harness.clock.snapshot().light);

  harness.advanceSeconds(30);
  TEST_ASSERT_EQUAL(Core::Phase::Idle, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT16(2, harness.clock.snapshot().endNumber);
}

void test_next_end_from_finished_does_not_skip_into_the_break() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  config.breakEnabled = true;
  config.breakAfterEnds = 1;
  config.breakMs = 30000;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(12);
  harness.clock.stop(harness.now);
  harness.clock.nextEnd(harness.now);

  TEST_ASSERT_EQUAL(Core::Phase::Idle, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT16(2, harness.clock.snapshot().endNumber);
}

void test_start_during_a_break_begins_the_next_end() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  config.breakEnabled = true;
  config.breakAfterEnds = 1;
  config.breakMs = 60000;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(12);
  harness.clock.stop(harness.now);
  harness.clock.lineClear(harness.now);
  harness.clock.nextEnd(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Break, harness.clock.snapshot().phase);

  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT16(2, harness.clock.snapshot().endNumber);
}

void test_controls_that_the_phase_forbids_are_refused_and_logged() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);

  // Scoring cannot be signalled while the clock has never run.
  harness.clock.lineClear(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Idle, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(0, harness.drainSignals().size());
  TEST_ASSERT_TRUE(harness.sink.contains("\"code\":\"control_rejected\""));

  // Nor can a suspension be resumed when nothing was suspended.
  harness.clock.resume(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Idle, harness.clock.snapshot().phase);
}

void test_a_clock_that_runs_backwards_cannot_swallow_the_ten_second_phase() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);

  // A command stamped a few milliseconds later than the update that follows
  // it. Unsigned subtraction turns that into about 4.29 billion milliseconds,
  // which used to consume the whole line-occupation period on the very next
  // line of the loop - the ten seconds simply never happened.
  harness.clock.start(harness.now + 5);

  harness.clock.update(harness.now);

  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(10000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_TRUE(harness.sink.contains("clock_backwards"));

  // And time still runs normally afterwards.
  harness.now += 5;
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
}

void test_the_ten_second_period_runs_its_full_length() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);
  harness.clock.start(harness.now);

  // Art. 11.3.1: green comes ten seconds after the athletes are called to the
  // line, so the panel counts 10 down to 1 and only then turns green.
  for (uint32_t second = 0; second < 10; second++) {
    TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
    TEST_ASSERT_EQUAL(Core::Light::Red, harness.clock.snapshot().light);
    TEST_ASSERT_EQUAL_UINT32(10000 - second * 1000, harness.clock.snapshot().remainingMs);
    harness.advanceSeconds(1);
  }

  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Green, harness.clock.snapshot().light);
}

void test_light_always_follows_the_phase() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Announced, 3);

  TEST_ASSERT_EQUAL(Core::Light::Off, harness.clock.snapshot().light);
  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL(Core::Light::Red, harness.clock.snapshot().light);
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Light::Green, harness.clock.snapshot().light);
  harness.advanceSeconds(60);
  TEST_ASSERT_EQUAL(Core::Light::Yellow, harness.clock.snapshot().light);
  harness.advanceSeconds(30);
  TEST_ASSERT_EQUAL(Core::Light::Red, harness.clock.snapshot().light);
}

void test_rule_records_carry_the_inputs_of_the_period_calculation() {
  Harness harness;
  harness.configure(Core::Mode::IndividualNonAlternating, Rules::EventClass::Other, 6);

  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.2.1.2\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"what\":\"period\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"arrows\":6"));
  TEST_ASSERT_TRUE(harness.sink.contains("\"per_arrow_ms\":40000"));
  TEST_ASSERT_TRUE(harness.sink.contains("\"period_ms\":240000"));
  TEST_ASSERT_TRUE(harness.sink.contains("\"reason\":\"OTHER\""));
}


void test_art_11_2_3_2_individual_alternating_gives_each_athlete_20_s_per_arrow() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualAlternating;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  config.firstShooter = 1;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.drainSignals();

  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().shooter);
  TEST_ASSERT_EQUAL_UINT32(20000, harness.clock.snapshot().remainingMs);

  // The director presses start again as the first arrow goes: one action that
  // stops this athlete and starts the opponent.
  harness.advanceSeconds(6);
  harness.clock.addArrow(harness.now);
  harness.clock.start(harness.now);

  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().shooter);
  TEST_ASSERT_EQUAL_UINT32(20000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_EQUAL(Core::SignalCode::Start, harness.drainSignals()[0]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"what\":\"handoff\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.2.3.2\""));
}

void test_art_11_2_3_2_a_missed_handoff_is_covered_by_the_timeout() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualAlternating;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  harness.clock.configure(harness.now, config);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.drainSignals();

  // Nobody presses anything; the twenty seconds simply run out.
  harness.advanceSeconds(20);

  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().shooter);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(20000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_EQUAL(Core::SignalCode::Start, harness.drainSignals()[0]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"reason\":\"timeout\""));
}

void test_art_11_3_4_the_handoff_can_be_silent_but_a_timeout_never_is() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualAlternating;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  // Several matches on one field: no signal at the start of each period.
  config.signalEachAlternatingPeriod = false;
  harness.clock.configure(harness.now, config);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  harness.drainSignals();

  harness.advanceSeconds(5);
  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL_UINT32(0, harness.drainSignals().size());

  // Art. 11.2.3.2 still requires a signal when the time runs out.
  harness.advanceSeconds(20);
  TEST_ASSERT_EQUAL_UINT32(1, harness.drainSignals().size());
}

void test_alternating_ends_once_both_athletes_have_shot_their_arrows() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualAlternating;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  harness.clock.configure(harness.now, config);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);

  for (uint8_t turn = 0; turn < 6; turn++) {
    harness.advanceSeconds(3);
    harness.clock.addArrow(harness.now);
    harness.clock.start(harness.now);
  }

  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT8(3, harness.clock.snapshot().sideArrows[0]);
  TEST_ASSERT_EQUAL_UINT8(3, harness.clock.snapshot().sideArrows[1]);
}

void test_art_11_1_4_2_team_ends_are_six_arrows_and_mixed_team_four() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::TeamSimultaneous;
  config.arrowsPerEnd = 3;  // wrong on purpose
  config.abcdRotation = false;
  harness.clock.configure(harness.now, config);

  TEST_ASSERT_EQUAL_UINT8(6, harness.clock.snapshot().arrowsPerEnd);
  TEST_ASSERT_EQUAL_UINT32(120000, harness.clock.snapshot().periodMs);
  TEST_ASSERT_TRUE(harness.sink.contains("Art. 11.1.4.2"));

  config.mode = Core::Mode::MixedTeam;
  harness.clock.configure(harness.now, config);
  TEST_ASSERT_EQUAL_UINT8(4, harness.clock.snapshot().arrowsPerEnd);
  TEST_ASSERT_EQUAL_UINT32(80000, harness.clock.snapshot().periodMs);
}

void test_art_11_1_4_3_team_alternating_banks_each_team_s_remaining_time() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::TeamAlternating;
  config.abcdRotation = false;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().shooter);
  TEST_ASSERT_EQUAL_UINT32(120000, harness.clock.snapshot().remainingMs);

  // Team one shoots its three arrows in 25 s and the director hands over.
  harness.advanceSeconds(25);
  for (uint8_t arrow = 0; arrow < 3; arrow++) harness.clock.addArrow(harness.now);
  harness.clock.start(harness.now);

  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().shooter);
  TEST_ASSERT_EQUAL_UINT32(95000, harness.clock.snapshot().sideRemainingMs[0]);
  TEST_ASSERT_EQUAL_UINT32(120000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.1.4.3\""));

  // Team two takes 30 s, and team one resumes from what it banked.
  harness.advanceSeconds(30);
  for (uint8_t arrow = 0; arrow < 3; arrow++) harness.clock.addArrow(harness.now);
  harness.clock.start(harness.now);

  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().shooter);
  TEST_ASSERT_EQUAL_UINT32(95000, harness.clock.snapshot().remainingMs);
  TEST_ASSERT_EQUAL_UINT32(90000, harness.clock.snapshot().sideRemainingMs[1]);
}

void test_abcd_rotation_is_on_in_a_default_session() {
  Harness harness;
  Core::SessionConfig config;
  harness.clock.configure(harness.now, config);

  TEST_ASSERT_TRUE(harness.clock.config().abcdRotation);
  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().details);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);

  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);

  harness.clock.stop(harness.now);
  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().detail);
}

void test_art_11_2_3_1_ab_cd_rotation_gives_the_next_detail_ten_seconds() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 3;
  config.abcdRotation = true;
  config.details = 2;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);
  harness.drainSignals();

  // The first detail's time runs out: stop, then straight into the changeover.
  harness.advanceSeconds(90);

  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().detail);
  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT32(10000, harness.clock.snapshot().remainingMs);
  std::vector<Core::SignalCode> signals = harness.drainSignals();
  TEST_ASSERT_EQUAL(Core::SignalCode::Stop, signals[0]);
  TEST_ASSERT_EQUAL(Core::SignalCode::OccupyLine, signals[1]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"11.2.3.1\""));

  // The second detail shoots, and then the rotation is over.
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  harness.advanceSeconds(90);
  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().detail);
}

void test_art_11_2_3_1_the_starting_group_rotates_each_end() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 3;
  config.abcdRotation = true;
  config.details = 2;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);

  harness.advanceSeconds(10);
  harness.advanceSeconds(90);
  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().detail);
  harness.advanceSeconds(10);
  harness.advanceSeconds(90);
  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);

  harness.clock.lineClear(harness.now);
  harness.clock.nextEnd(harness.now);
  TEST_ASSERT_EQUAL_UINT16(2, harness.clock.snapshot().endNumber);
  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().detail);

  harness.clock.start(harness.now);
  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().detail);
  harness.advanceSeconds(10);
  harness.advanceSeconds(90);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);
  TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);

  harness.advanceSeconds(10);
  harness.advanceSeconds(90);
  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);

  harness.clock.lineClear(harness.now);
  harness.clock.nextEnd(harness.now);
  TEST_ASSERT_EQUAL_UINT16(3, harness.clock.snapshot().endNumber);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().detail);
}

void test_art_11_2_3_1_even_ends_still_give_ab_a_turn_after_cd() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 3;
  config.abcdRotation = true;
  config.details = 2;
  harness.clock.configure(harness.now, config);

  for (uint16_t end = 1; end <= 4; end++) {
    const uint8_t first = static_cast<uint8_t>(((end - 1) % 2) + 1);
    const uint8_t second = first == 1 ? 2 : 1;
    TEST_ASSERT_EQUAL_UINT16(end, harness.clock.snapshot().endNumber);

    harness.clock.start(harness.now);
    TEST_ASSERT_EQUAL_UINT8(first, harness.clock.snapshot().detail);
    TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);

    harness.clock.stop(harness.now);
    TEST_ASSERT_EQUAL(Core::Phase::Occupy, harness.clock.snapshot().phase);
    TEST_ASSERT_EQUAL_UINT8(second, harness.clock.snapshot().detail);

    harness.clock.stop(harness.now);
    TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
    TEST_ASSERT_EQUAL_UINT8(second, harness.clock.snapshot().detail);

    harness.clock.lineClear(harness.now);
    if (end < 4) harness.clock.nextEnd(harness.now);
  }
}

void test_chapter_14_practice_is_a_plain_start_and_stop() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::Practice;
  config.abcdRotation = false;
  config.practiceMs = 60000;
  harness.clock.configure(harness.now, config);

  TEST_ASSERT_EQUAL_UINT32(60000, harness.clock.snapshot().periodMs);
  harness.clock.start(harness.now);
  harness.advanceSeconds(10);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);

  // Chapter 14 describes only start and stop signals, so no yellow warning.
  harness.advanceSeconds(40);
  TEST_ASSERT_EQUAL(Core::Phase::Shooting, harness.clock.snapshot().phase);
  TEST_ASSERT_EQUAL(Core::Light::Green, harness.clock.snapshot().light);

  harness.advanceSeconds(20);
  TEST_ASSERT_EQUAL(Core::Phase::Finished, harness.clock.snapshot().phase);
}

void test_art_12_5_a_shoot_off_is_one_arrow_each_and_marked_as_its_own_end() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualNonAlternating;
  config.eventClass = Rules::EventClass::Announced;
  config.arrowsPerEnd = 6;
  config.abcdRotation = false;
  config.shootOff = true;
  harness.clock.configure(harness.now, config);

  TEST_ASSERT_TRUE(harness.clock.snapshot().shootOff);
  TEST_ASSERT_EQUAL_UINT8(1, harness.clock.snapshot().arrowsPerEnd);
  TEST_ASSERT_EQUAL_UINT32(30000, harness.clock.snapshot().periodMs);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"12.5\""));

  config.mode = Core::Mode::TeamSimultaneous;
  harness.clock.configure(harness.now, config);
  TEST_ASSERT_EQUAL_UINT8(3, harness.clock.snapshot().arrowsPerEnd);
  TEST_ASSERT_EQUAL_UINT32(60000, harness.clock.snapshot().periodMs);
}

void test_art_11_1_4_1_the_director_chooses_who_shoots_first() {
  Harness harness;
  Core::SessionConfig config;
  config.mode = Core::Mode::IndividualAlternating;
  config.arrowsPerEnd = 3;
  config.abcdRotation = false;
  config.firstShooter = 2;
  harness.clock.configure(harness.now, config);

  harness.clock.start(harness.now);
  harness.advanceSeconds(10);

  TEST_ASSERT_EQUAL_UINT8(2, harness.clock.snapshot().shooter);
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_art_11_2_1_1_announced_events_allow_30_s_per_arrow);
  RUN_TEST(test_art_11_2_1_2_other_events_allow_40_s_reducible_to_30);
  RUN_TEST(test_art_11_2_1_alternating_and_team_are_20_s_in_both_classes);
  RUN_TEST(test_art_11_2_1_period_is_arrows_times_per_arrow);
  RUN_TEST(test_art_11_3_1_full_sequence_of_lights_and_signals);
  RUN_TEST(test_art_11_3_1_stop_finishes_even_with_arrows_unshot);
  RUN_TEST(test_art_11_3_1_no_yellow_warning_in_alternating_shooting);
  RUN_TEST(test_art_11_2_4_1_individual_resume_is_a_flat_per_arrow_allowance);
  RUN_TEST(test_art_11_2_4_2_team_resume_keeps_a_clock_that_beats_the_floor);
  RUN_TEST(test_art_11_2_4_2_team_resume_lifts_a_clock_below_the_floor);
  RUN_TEST(test_art_11_2_4_resume_replays_the_ten_second_period_by_default);
  RUN_TEST(test_art_11_2_2_time_may_be_extended);
  RUN_TEST(test_art_11_3_3_emergency_gives_at_least_five_signals_from_any_phase);
  RUN_TEST(test_art_10_1_next_end_advances_and_clears_the_arrow_count);
  RUN_TEST(test_a_break_follows_scoring_after_the_configured_number_of_ends);
  RUN_TEST(test_next_end_from_finished_does_not_skip_into_the_break);
  RUN_TEST(test_start_during_a_break_begins_the_next_end);
  RUN_TEST(test_controls_that_the_phase_forbids_are_refused_and_logged);
  RUN_TEST(test_a_clock_that_runs_backwards_cannot_swallow_the_ten_second_phase);
  RUN_TEST(test_the_ten_second_period_runs_its_full_length);
  RUN_TEST(test_light_always_follows_the_phase);
  RUN_TEST(test_rule_records_carry_the_inputs_of_the_period_calculation);
  RUN_TEST(test_art_11_2_3_2_individual_alternating_gives_each_athlete_20_s_per_arrow);
  RUN_TEST(test_art_11_2_3_2_a_missed_handoff_is_covered_by_the_timeout);
  RUN_TEST(test_art_11_3_4_the_handoff_can_be_silent_but_a_timeout_never_is);
  RUN_TEST(test_alternating_ends_once_both_athletes_have_shot_their_arrows);
  RUN_TEST(test_art_11_1_4_2_team_ends_are_six_arrows_and_mixed_team_four);
  RUN_TEST(test_art_11_1_4_3_team_alternating_banks_each_team_s_remaining_time);
  RUN_TEST(test_abcd_rotation_is_on_in_a_default_session);
  RUN_TEST(test_art_11_2_3_1_ab_cd_rotation_gives_the_next_detail_ten_seconds);
  RUN_TEST(test_art_11_2_3_1_the_starting_group_rotates_each_end);
  RUN_TEST(test_art_11_2_3_1_even_ends_still_give_ab_a_turn_after_cd);
  RUN_TEST(test_chapter_14_practice_is_a_plain_start_and_stop);
  RUN_TEST(test_art_12_5_a_shoot_off_is_one_arrow_each_and_marked_as_its_own_end);
  RUN_TEST(test_art_11_1_4_1_the_director_chooses_who_shoots_first);
  return UNITY_END();
}
