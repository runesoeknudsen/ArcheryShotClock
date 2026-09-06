#include <unity.h>

#include <string>
#include <vector>

#include "core/match_logic.h"

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

  Core::MatchLogic match{tracer};
  uint32_t now = 1000;

  void configure(Core::Division division, bool team, uint8_t arrows) {
    Core::MatchConfig config;
    config.division = division;
    config.scoring = Core::defaultScoring(division);
    config.team = team;
    config.arrowsPerEnd = arrows;
    match.configure(now, config);
  }

  // Scores one end for both sides and closes it.
  void shootEnd(const std::vector<uint8_t>& sideA, const std::vector<uint8_t>& sideB) {
    for (uint8_t value : sideA) match.recordArrow(now, 0, value);
    for (uint8_t value : sideB) match.recordArrow(now, 1, value);
    match.completeEnd(now);
  }
};

}  // namespace

void test_art_12_1_4_division_chooses_the_scoring_system() {
  TEST_ASSERT_EQUAL(Core::Scoring::SetPlay, Core::defaultScoring(Core::Division::Recurve));
  TEST_ASSERT_EQUAL(Core::Scoring::SetPlay, Core::defaultScoring(Core::Division::Barebow));
  TEST_ASSERT_EQUAL(Core::Scoring::Cumulative, Core::defaultScoring(Core::Division::Compound));
}

void test_art_12_5_1_x_is_an_inner_ten_and_a_miss_scores_nothing() {
  TEST_ASSERT_EQUAL_UINT8(10, Core::arrowScore(Core::ARROW_X));
  TEST_ASSERT_EQUAL_UINT8(10, Core::arrowScore(10));
  TEST_ASSERT_EQUAL_UINT8(0, Core::arrowScore(Core::ARROW_MISS));
  TEST_ASSERT_EQUAL_UINT8(7, Core::arrowScore(7));
}

void test_art_12_1_4_1_two_set_points_for_the_higher_score_one_each_when_tied() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);

  harness.shootEnd({10, 9, 8}, {9, 9, 8});
  TEST_ASSERT_EQUAL_UINT8(2, harness.match.state().setPoints[0]);
  TEST_ASSERT_EQUAL_UINT8(0, harness.match.state().setPoints[1]);

  harness.shootEnd({9, 9, 9}, {9, 9, 9});
  TEST_ASSERT_EQUAL_UINT8(3, harness.match.state().setPoints[0]);
  TEST_ASSERT_EQUAL_UINT8(1, harness.match.state().setPoints[1]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"12.1.4.1\""));
}

void test_art_12_1_4_1_six_set_points_wins_an_individual_match() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);

  harness.shootEnd({10, 10, 10}, {9, 9, 9});
  harness.shootEnd({10, 10, 10}, {9, 9, 9});
  TEST_ASSERT_EQUAL(Core::Outcome::Undecided, harness.match.state().outcome);

  harness.shootEnd({10, 10, 10}, {9, 9, 9});

  TEST_ASSERT_EQUAL_UINT8(6, harness.match.state().setPoints[0]);
  TEST_ASSERT_EQUAL(Core::Outcome::SideA, harness.match.state().outcome);
  TEST_ASSERT_TRUE(harness.sink.contains("\"what\":\"winner\""));
}

void test_art_12_1_4_2_five_set_points_wins_a_team_match() {
  Harness harness;
  harness.configure(Core::Division::Recurve, true, 6);

  harness.shootEnd({10, 10, 10, 10, 10, 10}, {9, 9, 9, 9, 9, 9});
  harness.shootEnd({10, 10, 10, 10, 10, 10}, {9, 9, 9, 9, 9, 9});
  TEST_ASSERT_EQUAL_UINT8(4, harness.match.state().setPoints[0]);
  TEST_ASSERT_EQUAL(Core::Outcome::Undecided, harness.match.state().outcome);

  harness.shootEnd({9, 9, 9, 9, 9, 9}, {9, 9, 9, 9, 9, 9});

  TEST_ASSERT_EQUAL_UINT8(5, harness.match.state().setPoints[0]);
  TEST_ASSERT_EQUAL(Core::Outcome::SideA, harness.match.state().outcome);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"12.1.4.2\""));
}

void test_a_match_already_won_does_not_keep_scoring() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);
  for (uint8_t end = 0; end < 3; end++) harness.shootEnd({10, 10, 10}, {9, 9, 9});
  TEST_ASSERT_EQUAL(Core::Outcome::SideA, harness.match.state().outcome);

  const uint8_t setPointsBefore = harness.match.state().setPoints[1];
  harness.shootEnd({1, 1, 1}, {10, 10, 10});

  TEST_ASSERT_EQUAL_UINT8(setPointsBefore, harness.match.state().setPoints[1]);
  TEST_ASSERT_EQUAL(Core::Outcome::SideA, harness.match.state().outcome);
  TEST_ASSERT_TRUE(harness.sink.contains("match_decided"));
}

void test_art_12_1_4_3_individual_cumulative_is_decided_after_five_ends() {
  Harness harness;
  harness.configure(Core::Division::Compound, false, 3);

  for (uint8_t end = 0; end < 4; end++) harness.shootEnd({10, 10, 9}, {10, 9, 9});
  TEST_ASSERT_EQUAL(Core::Outcome::Undecided, harness.match.state().outcome);
  TEST_ASSERT_EQUAL_UINT16(116, harness.match.state().runningTotal[0]);

  harness.shootEnd({10, 10, 9}, {10, 9, 9});

  TEST_ASSERT_EQUAL(Core::Outcome::SideA, harness.match.state().outcome);
  TEST_ASSERT_EQUAL_UINT16(145, harness.match.state().runningTotal[0]);
  TEST_ASSERT_EQUAL_UINT16(140, harness.match.state().runningTotal[1]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"12.1.4.3\""));
}

void test_art_12_1_4_4_team_cumulative_is_decided_after_four_ends() {
  Harness harness;
  harness.configure(Core::Division::Compound, true, 6);

  for (uint8_t end = 0; end < 3; end++) harness.shootEnd({10, 10, 10, 10, 10, 9}, {10, 10, 10, 10, 9, 9});
  TEST_ASSERT_EQUAL(Core::Outcome::Undecided, harness.match.state().outcome);

  harness.shootEnd({10, 10, 10, 10, 10, 9}, {10, 10, 10, 10, 9, 9});

  TEST_ASSERT_EQUAL(Core::Outcome::SideA, harness.match.state().outcome);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"12.1.4.4\""));
}

void test_a_level_cumulative_match_is_reported_as_drawn_not_guessed() {
  Harness harness;
  harness.configure(Core::Division::Compound, false, 3);

  for (uint8_t end = 0; end < 5; end++) harness.shootEnd({10, 9, 8}, {9, 9, 9});

  // Book 3 says only that the highest total wins, and gives no tie-break for a
  // cumulative match that ends level.
  TEST_ASSERT_EQUAL(Core::Outcome::Drawn, harness.match.state().outcome);
  TEST_ASSERT_TRUE(harness.sink.contains("Book 3 gives no tie-break"));
}

void test_art_13_3_forfeits_the_highest_arrow_and_keeps_it_on_the_card() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);
  harness.match.recordArrow(harness.now, 0, 10);
  harness.match.recordArrow(harness.now, 0, 8);
  harness.match.recordArrow(harness.now, 0, 7);
  TEST_ASSERT_EQUAL_UINT16(25, harness.match.state().endTotal[0]);

  // An arrow shot after the stop signal: the highest arrow of the end is lost,
  // not the offending one.
  TEST_ASSERT_TRUE(harness.match.forfeitHighest(harness.now, 0, "13.3"));

  TEST_ASSERT_EQUAL_UINT16(15, harness.match.state().endTotal[0]);
  TEST_ASSERT_EQUAL_UINT8(3, harness.match.state().arrowCount[0]);
  TEST_ASSERT_EQUAL_UINT8(10, harness.match.state().arrows[0][0]);
  TEST_ASSERT_TRUE(harness.match.state().forfeited[0][0]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"lost_value\":10"));
}

void test_art_13_6_2_an_uncorrected_yellow_card_costs_the_highest_arrow() {
  Harness harness;
  harness.configure(Core::Division::Recurve, true, 6);
  harness.match.yellowCard(harness.now, 1);
  TEST_ASSERT_TRUE(harness.match.state().yellowCard[1]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"13.6.1\""));

  for (uint8_t index = 0; index < 6; index++) harness.match.recordArrow(harness.now, 1, 9);
  harness.match.forfeitHighest(harness.now, 1, "13.6.2");

  TEST_ASSERT_EQUAL_UINT16(45, harness.match.state().endTotal[1]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"13.6.2\""));
}

void test_art_12_2_2_3_unshot_team_arrows_score_as_misses() {
  Harness harness;
  harness.configure(Core::Division::Recurve, true, 6);

  harness.match.recordArrow(harness.now, 0, 10);
  harness.match.recordArrow(harness.now, 0, 10);
  harness.match.recordArrow(harness.now, 0, 10);
  harness.match.recordArrow(harness.now, 0, 10);
  for (uint8_t index = 0; index < 6; index++) harness.match.recordArrow(harness.now, 1, 5);
  harness.match.completeEnd(harness.now);

  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"12.2.2.3\""));
  TEST_ASSERT_TRUE(harness.sink.contains("\"unshot\":2"));
  // Forty from four tens, and the two unshot arrows add nothing.
  TEST_ASSERT_EQUAL_UINT16(40, harness.match.state().runningTotal[0]);
  TEST_ASSERT_EQUAL_UINT8(2, harness.match.state().setPoints[0]);
}

void test_an_individual_short_end_is_flagged_rather_than_filled_with_misses() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);

  harness.match.recordArrow(harness.now, 0, 10);
  harness.match.recordArrow(harness.now, 1, 9);
  harness.match.recordArrow(harness.now, 1, 9);
  harness.match.recordArrow(harness.now, 1, 9);
  harness.match.completeEnd(harness.now);

  // Art. 12.2.2.3 covers team match play only; Book 3 has no equivalent for an
  // individual, so nothing is invented.
  TEST_ASSERT_FALSE(harness.sink.contains("\"art\":\"12.2.2.3\""));
  TEST_ASSERT_TRUE(harness.sink.contains("no miss rule"));
  TEST_ASSERT_EQUAL_UINT16(10, harness.match.state().runningTotal[0]);
}

void test_art_12_2_2_refuses_more_arrows_than_the_end_allows() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);
  for (uint8_t index = 0; index < 3; index++) harness.match.recordArrow(harness.now, 0, 9);

  TEST_ASSERT_FALSE(harness.match.recordArrow(harness.now, 0, 10));
  TEST_ASSERT_EQUAL_UINT8(3, harness.match.state().arrowCount[0]);
  TEST_ASSERT_TRUE(harness.sink.contains("Art. 12.2.2"));
}

void test_art_13_4_warnings_are_counted_and_the_second_is_noted() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);

  harness.match.warn(harness.now, 0);
  TEST_ASSERT_EQUAL_UINT8(1, harness.match.state().warnings[0]);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"13.4\""));

  harness.match.warn(harness.now, 0);
  TEST_ASSERT_EQUAL_UINT8(2, harness.match.state().warnings[0]);
  TEST_ASSERT_TRUE(harness.sink.contains("already warned"));
  // Still only a record: disqualification is the judge's to declare.
  TEST_ASSERT_EQUAL(Core::Outcome::Undecided, harness.match.state().outcome);
}

void test_art_13_5_disqualification_decides_the_match_for_the_opponent() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);

  harness.match.disqualify(harness.now, 0);

  TEST_ASSERT_TRUE(harness.match.state().disqualified[0]);
  TEST_ASSERT_EQUAL(Core::Outcome::SideB, harness.match.state().outcome);
  TEST_ASSERT_TRUE(harness.sink.contains("\"art\":\"13.5\""));
}

void test_arrow_values_outside_the_scoring_range_are_refused() {
  Harness harness;
  harness.configure(Core::Division::Recurve, false, 3);

  TEST_ASSERT_FALSE(harness.match.recordArrow(harness.now, 0, 12));
  TEST_ASSERT_EQUAL_UINT8(0, harness.match.state().arrowCount[0]);
  TEST_ASSERT_TRUE(harness.match.recordArrow(harness.now, 0, Core::ARROW_X));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_art_12_1_4_division_chooses_the_scoring_system);
  RUN_TEST(test_art_12_5_1_x_is_an_inner_ten_and_a_miss_scores_nothing);
  RUN_TEST(test_art_12_1_4_1_two_set_points_for_the_higher_score_one_each_when_tied);
  RUN_TEST(test_art_12_1_4_1_six_set_points_wins_an_individual_match);
  RUN_TEST(test_art_12_1_4_2_five_set_points_wins_a_team_match);
  RUN_TEST(test_a_match_already_won_does_not_keep_scoring);
  RUN_TEST(test_art_12_1_4_3_individual_cumulative_is_decided_after_five_ends);
  RUN_TEST(test_art_12_1_4_4_team_cumulative_is_decided_after_four_ends);
  RUN_TEST(test_a_level_cumulative_match_is_reported_as_drawn_not_guessed);
  RUN_TEST(test_art_13_3_forfeits_the_highest_arrow_and_keeps_it_on_the_card);
  RUN_TEST(test_art_13_6_2_an_uncorrected_yellow_card_costs_the_highest_arrow);
  RUN_TEST(test_art_12_2_2_3_unshot_team_arrows_score_as_misses);
  RUN_TEST(test_an_individual_short_end_is_flagged_rather_than_filled_with_misses);
  RUN_TEST(test_art_12_2_2_refuses_more_arrows_than_the_end_allows);
  RUN_TEST(test_art_13_4_warnings_are_counted_and_the_second_is_noted);
  RUN_TEST(test_art_13_5_disqualification_decides_the_match_for_the_opponent);
  RUN_TEST(test_arrow_values_outside_the_scoring_range_are_refused);
  return UNITY_END();
}
