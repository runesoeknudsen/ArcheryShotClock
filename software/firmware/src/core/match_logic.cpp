#include "match_logic.h"

namespace Core {

const char* name(Outcome outcome) {
  switch (outcome) {
    case Outcome::Undecided: return "UNDECIDED";
    case Outcome::SideA: return "A";
    case Outcome::SideB: return "B";
    case Outcome::Drawn: return "DRAWN";
  }
  return "UNKNOWN";
}

const char* name(Division division) {
  switch (division) {
    case Division::Recurve: return "RECURVE";
    case Division::Barebow: return "BAREBOW";
    case Division::Compound: return "COMPOUND";
  }
  return "UNKNOWN";
}

const char* name(Scoring scoring) {
  switch (scoring) {
    case Scoring::SetPlay: return "SET_PLAY";
    case Scoring::Cumulative: return "CUMULATIVE";
  }
  return "UNKNOWN";
}

uint8_t arrowScore(uint8_t value) {
  if (value == ARROW_X) return 10;
  return value > 10 ? 0 : value;
}

Scoring defaultScoring(Division division) {
  // Art. 12.1.4, stated plainly enough to encode directly.
  return division == Division::Compound ? Scoring::Cumulative : Scoring::SetPlay;
}

MatchLogic::MatchLogic(Tracer& tracer) : tracer_(tracer), enabled_(false) {}

void MatchLogic::enable(uint32_t now, bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  tracer_.config(now, "match_logic", enabled ? 0 : 1, enabled ? 1 : 0);
}

void MatchLogic::configure(uint32_t now, const MatchConfig& config) {
  config_ = config;
  if (config_.arrowsPerEnd > ARROWS_MAX) config_.arrowsPerEnd = ARROWS_MAX;

  const TraceField fields[] = {
      {"arrows_per_end", config_.arrowsPerEnd},
      {"set_points_to_win", setPointsToWin()},
      {"cumulative_ends", cumulativeEnds()},
  };
  tracer_.rule(now, "12.1.4", "scoring_system", fields, 3, name(config_.scoring));
  reset(now);
}

uint8_t MatchLogic::setPointsToWin() const {
  return config_.team ? SET_POINTS_TO_WIN_TEAM : SET_POINTS_TO_WIN_INDIVIDUAL;
}

uint8_t MatchLogic::cumulativeEnds() const {
  return config_.team ? CUMULATIVE_ENDS_TEAM : CUMULATIVE_ENDS_INDIVIDUAL;
}

void MatchLogic::reset(uint32_t now) {
  state_ = MatchState();
  tracer_.config(now, "match_reset", 0, 1);
}

uint16_t MatchLogic::totalFor(uint8_t side) const {
  uint16_t total = 0;
  for (uint8_t index = 0; index < state_.arrowCount[side]; index++) {
    if (state_.forfeited[side][index]) continue;
    total += arrowScore(state_.arrows[side][index]);
  }
  return total;
}

bool MatchLogic::recordArrow(uint32_t now, uint8_t side, uint8_t value) {
  if (side > 1) return false;
  if (value > 10 && value != ARROW_X) {
    tracer_.warn(now, "arrow_value", "outside 0-10 and X");
    return false;
  }
  if (state_.arrowCount[side] >= config_.arrowsPerEnd) {
    // Art. 12.2.2 handles surplus arrows by scoring only the lowest values, a
    // judgement made at the target. The console refuses the entry rather than
    // guessing which arrows the judge would keep.
    tracer_.warn(now, "arrow_count", "more than the end allows (Art. 12.2.2)");
    return false;
  }

  state_.arrows[side][state_.arrowCount[side]] = value;
  state_.forfeited[side][state_.arrowCount[side]] = false;
  state_.arrowCount[side]++;
  state_.endTotal[side] = totalFor(side);

  const TraceField fields[] = {{"side", side}, {"value", value}, {"end_total", state_.endTotal[side]}};
  tracer_.rule(now, "12.1.3", "arrow", fields, 3, value == ARROW_X ? "X" : (value == ARROW_MISS ? "M" : nullptr));
  return true;
}

bool MatchLogic::removeLastArrow(uint32_t now, uint8_t side) {
  if (side > 1 || state_.arrowCount[side] == 0) return false;
  state_.arrowCount[side]--;
  state_.endTotal[side] = totalFor(side);
  tracer_.config(now, side == 0 ? "arrows_a" : "arrows_b", state_.arrowCount[side] + 1, state_.arrowCount[side]);
  return true;
}

bool MatchLogic::forfeitHighest(uint32_t now, uint8_t side, const char* article) {
  if (side > 1 || state_.arrowCount[side] == 0) return false;

  uint8_t best = 0xFF;
  uint8_t bestScore = 0;
  for (uint8_t index = 0; index < state_.arrowCount[side]; index++) {
    if (state_.forfeited[side][index]) continue;
    const uint8_t score = arrowScore(state_.arrows[side][index]);
    if (best == 0xFF || score > bestScore) {
      best = index;
      bestScore = score;
    }
  }
  if (best == 0xFF) return false;

  state_.forfeited[side][best] = true;
  const uint16_t before = state_.endTotal[side];
  state_.endTotal[side] = totalFor(side);

  const TraceField fields[] = {
      {"side", side}, {"arrow_index", best}, {"lost_value", bestScore},
      {"before", before}, {"after", state_.endTotal[side]},
  };
  // "The scorer will enter the values of all arrows of that end, but the
  // highest-scoring arrow will be forfeited" - so the arrow stays recorded and
  // is marked, not deleted.
  tracer_.rule(now, article, "forfeit_highest", fields, 5, "judge decision");
  return true;
}

void MatchLogic::yellowCard(uint32_t now, uint8_t side) {
  if (side > 1) return;
  state_.yellowCard[side] = true;
  const TraceField fields[] = {{"side", side}};
  tracer_.rule(now, "13.6.1", "yellow_card", fields, 1, "athlete must restart behind the 1 m line");
}

void MatchLogic::clearYellowCard(uint32_t now, uint8_t side) {
  if (side > 1 || !state_.yellowCard[side]) return;
  state_.yellowCard[side] = false;
  tracer_.config(now, side == 0 ? "yellow_a" : "yellow_b", 1, 0);
}

void MatchLogic::warn(uint32_t now, uint8_t side) {
  if (side > 1) return;
  state_.warnings[side]++;
  const TraceField fields[] = {{"side", side}, {"warnings", state_.warnings[side]}};
  // Art. 13.4: one warning is allowed; continuing after it means
  // disqualification, which is a judge's call and so is not applied here.
  tracer_.rule(now, "13.4", "warning", fields, 2,
               state_.warnings[side] > 1 ? "already warned - Art. 13.4 allows disqualification" : nullptr);
}

void MatchLogic::disqualify(uint32_t now, uint8_t side) {
  if (side > 1) return;
  state_.disqualified[side] = true;
  const TraceField fields[] = {{"side", side}};
  tracer_.rule(now, "13.5", "disqualified", fields, 1, "judge decision");
  declare(now, side == 0 ? Outcome::SideB : Outcome::SideA, "13.5", "opponent disqualified");
}

void MatchLogic::fillUnshotArrows(uint32_t now) {
  // Art. 12.2.2.3 applies to team and mixed team match play only. There is no
  // equivalent article for individual matches, so an individual's unshot arrows
  // are left alone and flagged rather than silently scored as misses.
  for (uint8_t side = 0; side < 2; side++) {
    if (state_.arrowCount[side] >= config_.arrowsPerEnd) continue;

    if (!config_.team) {
      tracer_.warn(now, "arrows_unshot", "individual end short - Book 3 has no miss rule for this");
      continue;
    }

    const uint8_t missing = static_cast<uint8_t>(config_.arrowsPerEnd - state_.arrowCount[side]);
    while (state_.arrowCount[side] < config_.arrowsPerEnd) {
      state_.arrows[side][state_.arrowCount[side]] = ARROW_MISS;
      state_.forfeited[side][state_.arrowCount[side]] = false;
      state_.arrowCount[side]++;
    }
    const TraceField fields[] = {{"side", side}, {"unshot", missing}};
    tracer_.rule(now, "12.2.2.3", "unshot_as_miss", fields, 2, "team match play");
  }
}

void MatchLogic::declare(uint32_t now, Outcome outcome, const char* article, const char* reason) {
  state_.outcome = outcome;
  const TraceField fields[] = {
      {"set_points_a", state_.setPoints[0]},
      {"set_points_b", state_.setPoints[1]},
      {"total_a", state_.runningTotal[0]},
      {"total_b", state_.runningTotal[1]},
  };
  tracer_.rule(now, article, outcome == Outcome::Drawn ? "drawn" : "winner", fields, 4, reason);
}

void MatchLogic::completeEnd(uint32_t now) {
  if (state_.outcome != Outcome::Undecided) {
    tracer_.warn(now, "match_decided", "end ignored, the match is already won");
    return;
  }

  fillUnshotArrows(now);
  state_.endTotal[0] = totalFor(0);
  state_.endTotal[1] = totalFor(1);
  state_.runningTotal[0] += state_.endTotal[0];
  state_.runningTotal[1] += state_.endTotal[1];

  if (config_.scoring == Scoring::SetPlay) {
    uint8_t awarded[2] = {0, 0};
    if (state_.endTotal[0] > state_.endTotal[1]) {
      awarded[0] = SET_POINTS_WIN;
    } else if (state_.endTotal[1] > state_.endTotal[0]) {
      awarded[1] = SET_POINTS_WIN;
    } else {
      awarded[0] = SET_POINTS_TIE;
      awarded[1] = SET_POINTS_TIE;
    }
    state_.setPoints[0] += awarded[0];
    state_.setPoints[1] += awarded[1];

    const TraceField fields[] = {
        {"end", state_.endNumber},   {"total_a", state_.endTotal[0]}, {"total_b", state_.endTotal[1]},
        {"awarded_a", awarded[0]},   {"awarded_b", awarded[1]},       {"set_points_a", state_.setPoints[0]},
        {"set_points_b", state_.setPoints[1]},
    };
    tracer_.rule(now, config_.team ? "12.1.4.2" : "12.1.4.1", "set_points", fields, 7, nullptr);

    // "As soon as an athlete reaches 6 set points, the athlete is declared the
    // winner" - a threshold, not a fixed number of sets.
    const uint8_t target = setPointsToWin();
    if (state_.setPoints[0] >= target && state_.setPoints[1] >= target) {
      declare(now, Outcome::Drawn, config_.team ? "12.1.4.2" : "12.1.4.1", "both reached the threshold");
    } else if (state_.setPoints[0] >= target) {
      declare(now, Outcome::SideA, config_.team ? "12.1.4.2" : "12.1.4.1", "reached the set point threshold");
    } else if (state_.setPoints[1] >= target) {
      declare(now, Outcome::SideB, config_.team ? "12.1.4.2" : "12.1.4.1", "reached the set point threshold");
    }
  } else {
    const TraceField fields[] = {
        {"end", state_.endNumber},
        {"total_a", state_.endTotal[0]},
        {"total_b", state_.endTotal[1]},
        {"running_a", state_.runningTotal[0]},
        {"running_b", state_.runningTotal[1]},
    };
    tracer_.rule(now, config_.team ? "12.1.4.4" : "12.1.4.3", "cumulative", fields, 5, nullptr);

    if (state_.endNumber >= cumulativeEnds()) {
      if (state_.runningTotal[0] > state_.runningTotal[1]) {
        declare(now, Outcome::SideA, config_.team ? "12.1.4.4" : "12.1.4.3", "highest total");
      } else if (state_.runningTotal[1] > state_.runningTotal[0]) {
        declare(now, Outcome::SideB, config_.team ? "12.1.4.4" : "12.1.4.3", "highest total");
      } else {
        // Book 3 says only that the highest total wins. It gives no tie-break
        // for a cumulative match that finishes level, so the system reports the
        // draw and leaves the decision to the officials.
        declare(now, Outcome::Drawn, config_.team ? "12.1.4.4" : "12.1.4.3", "level - Book 3 gives no tie-break");
      }
    }
  }

  state_.endNumber++;
  state_.arrowCount[0] = 0;
  state_.arrowCount[1] = 0;
  state_.endTotal[0] = 0;
  state_.endTotal[1] = 0;
  for (uint8_t side = 0; side < 2; side++) {
    for (uint8_t index = 0; index < ARROWS_MAX; index++) state_.forfeited[side][index] = false;
  }
}

}  // namespace Core
