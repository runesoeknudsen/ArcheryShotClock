#pragma once

#include <cstdint>

#include "rules.h"
#include "signals.h"
#include "snapshot.h"
#include "trace.h"

// The World Archery shooting sequence as a state machine.
//
//   IDLE --start--> OCCUPY --10 s--> SHOOTING --30 s left--> WARNING
//                                        |                      |
//                                        +---- stop / 0 --------+--> FINISHED
//                                                                      |
//                                              line clear (Art. 11.3.2) v
//                                                                   SCORING --next end--> IDLE
//                                                                      |
//                                              optional break after N ends v
//                                                                     BREAK --end / timeout--> IDLE
//
// SUSPENDED (Art. 11.2.4) and EMERGENCY (Art. 11.3.3) can be entered from any
// running phase and return through their own transitions.
//
// The clock is authoritative. Art. 11.3.1 says so outright - "If there is a
// discrepancy, the digital clock takes precedence" - so light colour is derived
// from the phase and the countdown here, never tracked as separate state that
// could drift.
//
// Every decision this class makes is written to the trace as a RULE record
// carrying the inputs it was made from, so tools/logcheck.py can recompute it
// and disagree.

namespace Core {

struct SessionConfig {
  Mode mode = Mode::IndividualNonAlternating;
  Rules::EventClass eventClass = Rules::EventClass::Other;
  uint8_t arrowsPerEnd = Rules::ARROWS_PER_END_SHORT;

  // Art. 11.2.4 says the recalculated allowance is "determined by the number of
  // arrows not shot, including the 10-second signal", while Art. 11.3.3 gives
  // "one sound signal ... for shooting to continue". The book never states
  // whether the two-signal line-occupation sequence is re-run on resumption, so
  // this is an interpretation the director can switch, not a rule. Default is
  // to replay the 10 s period, following 11.2.4's wording.
  bool replayOccupyOnResume = true;

  // Art. 11.1.4.1 / 11.1.4.3: the higher-placed athlete or team decides who
  // shoots first. 1 or 2.
  uint8_t firstShooter = 1;

  // Art. 11.3.4: where several matches share a field with alternating
  // shooting, no sound signal may be given at the start of each shooting
  // period, only at the start of the match. Turn this off in that situation.
  bool signalEachAlternatingPeriod = true;

  // Art. 11.2.3.1: AB/CD detail rotation, with ten seconds for one detail to
  // leave and the next to occupy the line. Qualification defaults to this.
  bool abcdRotation = true;
  uint8_t details = 2;

  // Art. 12.5: run this end as a shoot-off - one arrow for an individual,
  // one per athlete for a team - outside the normal end and set count.
  bool shootOff = false;

  // Chapter 14 gives the practice session no per-arrow structure, only a
  // start and a stop, so its length is the director's to set.
  uint32_t practiceMs = 300000;

  // Optional pause after scoring. On by default: after every N ends the
  // director gets a timed break before the next end can start. Zero ends
  // disables it even when the flag is on.
  bool breakEnabled = true;
  uint8_t breakAfterEnds = 12;
  uint32_t breakMs = 15 * 60 * 1000;
};

class ShotClock {
public:
  explicit ShotClock(Tracer& tracer);

  void configure(uint32_t now, const SessionConfig& config);
  const SessionConfig& config() const { return config_; }
  const StateSnapshot& snapshot() const { return state_; }
  uint32_t perArrowMs() const;

  // Director controls. Each is a no-op, logged as a WARN, when the current
  // phase does not allow it - a mis-press must never move the clock somewhere
  // the rulebook does not permit.
  // In alternating modes a press while the clock runs is the handoff, not a
  // new start (the same physical control, per the rulebook's single "start
  // shooting" signal). The engine tracks whose turn it is so the director
  // never has to think about which press number they are on.
  void start(uint32_t now);
  void stop(uint32_t now);
  void lineClear(uint32_t now);
  void nextEnd(uint32_t now);
  void resetEnd(uint32_t now);
  void suspend(uint32_t now);
  void resume(uint32_t now);
  void addArrow(uint32_t now);
  void removeArrow(uint32_t now);
  void extendTime(uint32_t now, uint32_t extraMs);
  void emergency(uint32_t now);
  void clearEmergency(uint32_t now);
  void setDisplayContent(uint32_t now, DisplayContent content);
  // Mirrors the match module's totals into the snapshot, so the panel, the web
  // UI and the trace all read scores from the same object as the clock.
  void setMatchTotals(const uint16_t score[2], const uint8_t setPoints[2]);

  void update(uint32_t now);

  // Signals are queued rather than played directly so the sound layer, the
  // trace and any future remote unit all consume the same sequence.
  bool takeSignal(SignalCode& signal);

private:
  void enterPhase(uint32_t now, Phase phase, uint32_t durationMs);
  void beginEnd(uint32_t now);
  void beginShooting(uint32_t now, uint32_t durationMs, const char* article, const char* what);
  void beginShootingPeriod(uint32_t now);
  void handoff(uint32_t now, const char* reason);
  void finish(uint32_t now, const char* reason);
  uint8_t effectiveArrowsPerEnd() const;
  uint32_t totalPeriodMs() const;
  bool sideComplete(uint8_t side) const;
  bool allArrowsShot() const;
  void emitSignal(uint32_t now, SignalCode code, const char* article);
  void rejected(uint32_t now, const char* control);
  void applyLight();
  uint8_t unshotArrows() const;
  bool clockRunning() const;
  bool breakDue() const;
  void enterBreak(uint32_t now);
  void leaveBreak(uint32_t now, bool startShooting);
  uint8_t firstDetailThisEnd() const;
  uint8_t nextDetailAfter(uint8_t detail) const;
  bool moreDetailsThisEnd() const;
  void setDetailForThisEnd();

  Tracer& tracer_;
  SessionConfig config_;
  StateSnapshot state_;
  uint32_t lastTick_;
  uint32_t pendingShootingMs_;

  // Anything longer than this between two updates is a clock that moved
  // backwards, not a genuinely slow loop. Half the millis() range, so a real
  // rollover is still handled correctly by the unsigned arithmetic.
  static constexpr uint32_t BACKWARDS_CLOCK_MS = 0x80000000u;

  static constexpr uint8_t QUEUE_SIZE = 8;
  SignalCode queue_[QUEUE_SIZE];
  uint8_t queueHead_;
  uint8_t queueTail_;
};

}  // namespace Core
