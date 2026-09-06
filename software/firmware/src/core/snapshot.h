#pragma once

#include <cstdint>

// The single state object the rest of the firmware reads from.
//
// The timing core produces one StateSnapshot; the display, the sound layer,
// the web UI and the UART trace are all just consumers of it. Keeping every
// consumer on the same struct is what makes the serial log a complete record
// of what the firmware believed at any moment, and what later allows a second
// device to be fed the same state over the air without reworking the core.
//
// Bump SCHEMA_VERSION whenever a field changes meaning. Log readers and, in
// future, remote units check it and refuse to guess.

namespace Core {

// 3: added the optional break after scoring, shown as its own phase.
constexpr uint16_t SCHEMA_VERSION = 3;

// Where the shooting sequence currently is. Phase 0 only ever reports Idle,
// Shooting and Finished; the remaining values are the World Archery sequence
// (Art. 11.3.1) that the timing engine fills in from Phase 1 onward.
enum class Phase : uint8_t {
  Idle,
  Occupy,     // 10 s line-occupation period, RED
  Shooting,   // GREEN, clock running
  Warning,    // last 30 s, YELLOW
  Finished,   // time expired or stopped, RED
  Scoring,    // scoring may begin, 3 signals given
  Break,      // optional pause after scoring, before the next end
  Suspended,  // Art. 11.2.4 suspension
  Emergency   // Art. 11.3.3, all shooting ceases
};

// The light athletes read. Art. 11.3.1 makes the digital clock authoritative
// if the two ever disagree, so this is always derived from the clock and never
// tracked as independent state.
enum class Light : uint8_t { Off, Red, Green, Yellow };

// Timing mode. Plain is the pre-rulebook countdown this firmware started as,
// kept so the device stays useful as an ordinary club timer.
enum class Mode : uint8_t {
  Plain,
  IndividualNonAlternating,
  IndividualAlternating,
  TeamSimultaneous,
  TeamAlternating,
  MixedTeam,
  ShootOff,
  Practice
};

// What the 32x16 panel is showing. Clock is the default; the web UI selects
// the rest, and everything stays visible in the web UI regardless.
enum class DisplayContent : uint8_t {
  Clock,
  ClockAndEnd,
  ArrowCount,
  Score,
  SetPoints,
  Shooter,
  Blank
};

struct StateSnapshot {
  uint16_t schemaVersion = SCHEMA_VERSION;

  Phase phase = Phase::Idle;
  Light light = Light::Off;
  Mode mode = Mode::Plain;
  DisplayContent display = DisplayContent::Clock;

  uint32_t remainingMs = 0;  // authoritative countdown
  uint32_t periodMs = 0;     // full length of the current period

  uint16_t endNumber = 0;
  uint16_t setNumber = 0;
  uint8_t arrowsShot = 0;    // needed for Art. 11.2.4 resume even with scoring off
  uint8_t arrowsPerEnd = 0;
  uint8_t shooter = 0;       // 0 = none, 1 = A/first, 2 = B/second

  uint16_t score[2] = {0, 0};
  uint8_t setPoints[2] = {0, 0};

  // Alternating shooting (Art. 11.1.4.1 and 11.1.4.3). Individual alternating
  // gives each athlete a fresh 20 s for one arrow; teams instead bank a whole
  // allowance that stops and starts, which is why the clocks are per side.
  uint8_t sideArrows[2] = {0, 0};
  uint32_t sideRemainingMs[2] = {0, 0};

  // AB/CD rotation (Art. 11.2.3.1): which detail is on the line.
  uint8_t detail = 1;
  uint8_t details = 1;

  // Art. 12.5: a shoot-off is a separate one-off end, not part of the normal
  // end or set count.
  bool shootOff = false;

  bool running = false;
  bool finished = false;
  bool soundEnabled = true;
  uint8_t brightness = 0;

  bool sameAs(const StateSnapshot& other) const {
    return schemaVersion == other.schemaVersion && phase == other.phase && light == other.light &&
           mode == other.mode && display == other.display && remainingMs == other.remainingMs &&
           periodMs == other.periodMs && endNumber == other.endNumber && setNumber == other.setNumber &&
           arrowsShot == other.arrowsShot && arrowsPerEnd == other.arrowsPerEnd && shooter == other.shooter &&
           score[0] == other.score[0] && score[1] == other.score[1] && setPoints[0] == other.setPoints[0] &&
           setPoints[1] == other.setPoints[1] && running == other.running && finished == other.finished &&
           soundEnabled == other.soundEnabled && brightness == other.brightness &&
           sideArrows[0] == other.sideArrows[0] &&
           sideArrows[1] == other.sideArrows[1] && sideRemainingMs[0] == other.sideRemainingMs[0] &&
           sideRemainingMs[1] == other.sideRemainingMs[1] && detail == other.detail && details == other.details &&
           shootOff == other.shootOff;
  }
};

inline const char* name(Phase value) {
  switch (value) {
    case Phase::Idle: return "IDLE";
    case Phase::Occupy: return "OCCUPY";
    case Phase::Shooting: return "SHOOTING";
    case Phase::Warning: return "WARNING";
    case Phase::Finished: return "FINISHED";
    case Phase::Scoring: return "SCORING";
    case Phase::Break: return "BREAK";
    case Phase::Suspended: return "SUSPENDED";
    case Phase::Emergency: return "EMERGENCY";
  }
  return "UNKNOWN";
}

inline const char* name(Light value) {
  switch (value) {
    case Light::Off: return "OFF";
    case Light::Red: return "RED";
    case Light::Green: return "GREEN";
    case Light::Yellow: return "YELLOW";
  }
  return "UNKNOWN";
}

inline const char* name(Mode value) {
  switch (value) {
    case Mode::Plain: return "PLAIN";
    case Mode::IndividualNonAlternating: return "IND_NONALT";
    case Mode::IndividualAlternating: return "IND_ALT";
    case Mode::TeamSimultaneous: return "TEAM_SIMUL";
    case Mode::TeamAlternating: return "TEAM_ALT";
    case Mode::MixedTeam: return "MIXED_TEAM";
    case Mode::ShootOff: return "SHOOT_OFF";
    case Mode::Practice: return "PRACTICE";
  }
  return "UNKNOWN";
}

inline const char* name(DisplayContent value) {
  switch (value) {
    case DisplayContent::Clock: return "CLOCK";
    case DisplayContent::ClockAndEnd: return "CLOCK_END";
    case DisplayContent::ArrowCount: return "ARROWS";
    case DisplayContent::Score: return "SCORE";
    case DisplayContent::SetPoints: return "SET_POINTS";
    case DisplayContent::Shooter: return "SHOOTER";
    case DisplayContent::Blank: return "BLANK";
  }
  return "UNKNOWN";
}

}  // namespace Core
