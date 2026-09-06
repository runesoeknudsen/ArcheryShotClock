#pragma once

#include <cstdint>

#include "snapshot.h"

// World Archery Book 3 - Target Archery, version 2026-01-27.
//
// Every constant here carries the article it comes from. Nothing in this file
// is a convention or a guess: where the rulebook is silent, the value lives in
// config.h as a project decision instead, and says so.
//
// This is an independent, unofficial implementation, not affiliated with or
// endorsed by World Archery.

namespace Rules {

// Art. 11.3.1: athletes occupy the line on RED, and the lights change to green
// "10 seconds later".
constexpr uint32_t OCCUPY_LINE_MS = 10000;

// Art. 11.3.1: "This warning signal will be given 30 seconds before the end of
// the time limit". Visual only - the article assigns no sound signal to yellow.
constexpr uint32_t WARNING_MS = 30000;

// Art. 11.2.1.1 / 11.2.1.2. Both classes allow 20 s per arrow for alternating
// individual and for all team and mixed team rounds; they differ only for
// individual shooting where alternate shooting does not apply.
constexpr uint32_t PER_ARROW_ALTERNATING_MS = 20000;
constexpr uint32_t PER_ARROW_INDIVIDUAL_ANNOUNCED_MS = 30000;  // Art. 11.2.1.1
constexpr uint32_t PER_ARROW_INDIVIDUAL_DEFAULT_MS = 40000;    // Art. 11.2.1.2
// Art. 11.2.1.2: "Organisers may reduce this time to 30 seconds per arrow by
// indicating so in the invitation."
constexpr uint32_t PER_ARROW_INDIVIDUAL_REDUCED_MS = 30000;

// Art. 10.1: "Each athlete will shoot arrows in ends of three or six arrows
// unless specified differently." The book states no other per-end count for
// individual competition, so both are offered and nothing else is.
constexpr uint8_t ARROWS_PER_END_SHORT = 3;
constexpr uint8_t ARROWS_PER_END_LONG = 6;

// Art. 11.3.1 and 11.3.3. These counts are load-bearing: officials on the
// field are trained to recognise them.
constexpr uint8_t SIGNALS_OCCUPY_LINE = 2;
constexpr uint8_t SIGNALS_START = 1;
constexpr uint8_t SIGNALS_STOP = 2;
constexpr uint8_t SIGNALS_SCORING = 3;
constexpr uint8_t SIGNALS_RESUME = 1;
// Art. 11.3.3: "a series of at least 5 sound signals".
constexpr uint8_t SIGNALS_EMERGENCY_MINIMUM = 5;

// Which timing class the event is run under. The distinction is Art. 11.2.1.1
// ("World Ranking Events and other national or international events in which
// the organisers announce beforehand") versus Art. 11.2.1.2 ("all other
// events"), and it changes nothing except the individual non-alternating rate.
enum class EventClass : uint8_t {
  Announced,       // Art. 11.2.1.1 - 30 s per arrow
  Other,           // Art. 11.2.1.2 - 40 s per arrow
  OtherReduced     // Art. 11.2.1.2 - reduced to 30 s in the invitation
};

inline const char* name(EventClass value) {
  switch (value) {
    case EventClass::Announced: return "ANNOUNCED";
    case EventClass::Other: return "OTHER";
    case EventClass::OtherReduced: return "OTHER_REDUCED";
  }
  return "UNKNOWN";
}

// The article that governs the per-arrow rate for this combination, so the
// decision can be logged with its source rather than just its result.
inline const char* perArrowArticle(Core::Mode mode, EventClass eventClass) {
  switch (mode) {
    case Core::Mode::IndividualNonAlternating:
      return eventClass == EventClass::Announced ? "11.2.1.1" : "11.2.1.2";
    default:
      // Alternating individual, team and mixed team are 20 s per arrow in both
      // classes, so either article supports it; report the one for the class.
      return eventClass == EventClass::Announced ? "11.2.1.1" : "11.2.1.2";
  }
}

inline uint32_t perArrowMs(Core::Mode mode, EventClass eventClass) {
  if (mode != Core::Mode::IndividualNonAlternating) return PER_ARROW_ALTERNATING_MS;
  switch (eventClass) {
    case EventClass::Announced: return PER_ARROW_INDIVIDUAL_ANNOUNCED_MS;
    case EventClass::Other: return PER_ARROW_INDIVIDUAL_DEFAULT_MS;
    case EventClass::OtherReduced: return PER_ARROW_INDIVIDUAL_REDUCED_MS;
  }
  return PER_ARROW_INDIVIDUAL_DEFAULT_MS;
}

// Art. 11.2.1: "The total time allowed to shoot an end will be determined by
// the total number of arrows to shoot in the end."
inline uint32_t periodMs(uint8_t arrows, uint32_t perArrow) {
  return static_cast<uint32_t>(arrows) * perArrow;
}

// Art. 11.1.4.2: a team shoots six arrows per end, a mixed team four.
constexpr uint8_t ARROWS_PER_END_TEAM = 6;
constexpr uint8_t ARROWS_PER_END_MIXED_TEAM = 4;

// Art. 12.5.2.1: a shoot-off is one arrow for an individual. Teams shoot one
// arrow per athlete - three, or two for a mixed team.
constexpr uint8_t SHOOT_OFF_ARROWS_INDIVIDUAL = 1;
constexpr uint8_t SHOOT_OFF_ARROWS_TEAM = 3;
constexpr uint8_t SHOOT_OFF_ARROWS_MIXED_TEAM = 2;

inline bool isTeam(Core::Mode mode) {
  return mode == Core::Mode::TeamSimultaneous || mode == Core::Mode::TeamAlternating ||
         mode == Core::Mode::MixedTeam;
}

inline bool isMixedTeam(Core::Mode mode) { return mode == Core::Mode::MixedTeam; }

inline bool isAlternating(Core::Mode mode) {
  return mode == Core::Mode::IndividualAlternating || mode == Core::Mode::TeamAlternating;
}

// Art. 11.1.4.3: a team hands over once every member has shot one arrow, so a
// turn is three arrows, or two for a mixed team. An individual alternating
// match hands over after every single arrow (Art. 11.2.3.2).
inline uint8_t arrowsPerTurn(Core::Mode mode) {
  if (mode == Core::Mode::TeamAlternating) return 3;
  return 1;
}

inline uint8_t defaultArrowsPerEnd(Core::Mode mode) {
  if (mode == Core::Mode::MixedTeam) return ARROWS_PER_END_MIXED_TEAM;
  if (isTeam(mode)) return ARROWS_PER_END_TEAM;
  return ARROWS_PER_END_SHORT;
}

inline uint8_t shootOffArrows(Core::Mode mode) {
  if (mode == Core::Mode::MixedTeam) return SHOOT_OFF_ARROWS_MIXED_TEAM;
  if (isTeam(mode)) return SHOOT_OFF_ARROWS_TEAM;
  return SHOOT_OFF_ARROWS_INDIVIDUAL;
}

// Art. 11.3.1 excludes the warning "in the Finals Round when the athletes shoot
// alternately". A 30 s warning inside a 20 s period could not be given anyway.
inline bool usesWarning(Core::Mode mode) {
  // Practice has no warning either: Chapter 14 describes only start and stop
  // signals, so inventing one would be adding a rule that is not there.
  return !isAlternating(mode) && mode != Core::Mode::Practice;
}

// Art. 11.2.4.1: on resumption an individual is "given 30 seconds per arrow or
// 40 seconds per arrow, depending on the type of event". Note what the article
// does not say: it never compares this against the time left on the clock, so
// unlike the team rule there is no "keep the larger" test here.
inline uint32_t individualResumeMs(uint8_t unshotArrows, uint32_t perArrow) {
  return periodMs(unshotArrows, perArrow);
}

// Art. 11.2.4.2: teams keep the clock if it holds more than 20 s per unshot
// arrow, otherwise they are given 20 s per unshot arrow. The article says
// "approximate total" and delegates the judgement to the director of shooting
// or the judge, so this is the arithmetic default a director may override.
inline uint32_t teamResumeMs(uint32_t clockMs, uint8_t unshotArrows) {
  const uint32_t floorMs = periodMs(unshotArrows, PER_ARROW_ALTERNATING_MS);
  return clockMs > floorMs ? clockMs : floorMs;
}

}  // namespace Rules
