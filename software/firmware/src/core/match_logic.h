#pragma once

#include <cstdint>

#include "trace.h"

// Chapter 12 scoring and the Chapter 13 consequences, as a module that can be
// switched off. The timing engine never depends on it: a director who only
// wants a shot clock should not be made to keep score.
//
// The system records decisions, it does not make them. Every Chapter 13
// consequence here is applied because a judge said so - nothing is detected.

namespace Core {

// Art. 12.1.4: "Recurve and Barebow will score using the set system, and
// Compound will be scored using a cumulative score."
enum class Division : uint8_t { Recurve, Barebow, Compound };
enum class Scoring : uint8_t { SetPlay, Cumulative };

// Art. 12.2.8: "A miss will be recorded as 'M' in the scorecard."
constexpr uint8_t ARROW_MISS = 0;
// X is the inner 10 (Art. 12.5.1). Book 3 never enumerates the arrow values or
// says what X scores - target faces and their zones live in Book 2 - so this
// sentinel is a project decision, not a quoted rule.
constexpr uint8_t ARROW_X = 11;
constexpr uint8_t ARROWS_MAX = 6;

// Art. 12.1.4.1 / 12.1.4.2: six set points win an individual match, five a
// team or mixed team match.
constexpr uint8_t SET_POINTS_TO_WIN_INDIVIDUAL = 6;
constexpr uint8_t SET_POINTS_TO_WIN_TEAM = 5;
// Art. 12.1.4.3 / 12.1.4.4: five ends individual, four team.
constexpr uint8_t CUMULATIVE_ENDS_INDIVIDUAL = 5;
constexpr uint8_t CUMULATIVE_ENDS_TEAM = 4;

// Art. 12.1.4.1: two set points to the higher score, one each if tied.
constexpr uint8_t SET_POINTS_WIN = 2;
constexpr uint8_t SET_POINTS_TIE = 1;

enum class Outcome : uint8_t { Undecided, SideA, SideB, Drawn };

const char* name(Outcome outcome);
const char* name(Division division);
const char* name(Scoring scoring);

// The score of one arrow. X counts as ten; a miss counts as nothing.
uint8_t arrowScore(uint8_t value);
Scoring defaultScoring(Division division);

struct MatchConfig {
  Division division = Division::Recurve;
  Scoring scoring = Scoring::SetPlay;
  bool team = false;
  uint8_t arrowsPerEnd = 3;
};

struct MatchState {
  uint16_t endNumber = 1;
  uint16_t endTotal[2] = {0, 0};
  uint16_t runningTotal[2] = {0, 0};
  uint8_t setPoints[2] = {0, 0};
  uint8_t arrowCount[2] = {0, 0};
  uint8_t arrows[2][ARROWS_MAX] = {{0}, {0}};
  bool forfeited[2][ARROWS_MAX] = {{false}, {false}};
  uint8_t warnings[2] = {0, 0};
  bool yellowCard[2] = {false, false};
  bool disqualified[2] = {false, false};
  Outcome outcome = Outcome::Undecided;
};

class MatchLogic {
public:
  explicit MatchLogic(Tracer& tracer);

  void configure(uint32_t now, const MatchConfig& config);
  const MatchConfig& config() const { return config_; }
  const MatchState& state() const { return state_; }

  void enable(uint32_t now, bool enabled);
  bool isEnabled() const { return enabled_; }

  // Art. 12.1.3: values are called out and entered in descending order. The
  // system accepts them in whatever order they arrive and does not enforce it -
  // the order is how people work at the target, not an arithmetic requirement.
  bool recordArrow(uint32_t now, uint8_t side, uint8_t value);
  bool removeLastArrow(uint32_t now, uint8_t side);

  // Art. 13.3 and 13.6.2: the offending arrow is not the one lost. The
  // highest-scoring arrow of the end is forfeited and scored as a miss, and
  // every arrow still appears on the card.
  bool forfeitHighest(uint32_t now, uint8_t side, const char* article);

  // Art. 13.6.1: a judge's card. Purely recorded; the system cannot see a
  // 1 m line being crossed.
  void yellowCard(uint32_t now, uint8_t side);
  void clearYellowCard(uint32_t now, uint8_t side);
  // Art. 13.4: one warning, then disqualification if the athlete continues.
  void warn(uint32_t now, uint8_t side);
  // Art. 13.5: no warning first.
  void disqualify(uint32_t now, uint8_t side);

  // Closes the end, awards set points or adds to the running total, and
  // declares a winner when the rulebook's threshold is reached.
  void completeEnd(uint32_t now);

  void reset(uint32_t now);

private:
  uint8_t setPointsToWin() const;
  uint8_t cumulativeEnds() const;
  uint16_t totalFor(uint8_t side) const;
  void fillUnshotArrows(uint32_t now);
  void declare(uint32_t now, Outcome outcome, const char* article, const char* reason);

  Tracer& tracer_;
  MatchConfig config_;
  MatchState state_;
  bool enabled_;
};

}  // namespace Core
