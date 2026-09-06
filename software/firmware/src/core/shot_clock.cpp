#include "shot_clock.h"

namespace Core {

ShotClock::ShotClock(Tracer& tracer)
    : tracer_(tracer), lastTick_(0), pendingShootingMs_(0), queueHead_(0), queueTail_(0) {
  for (uint8_t index = 0; index < QUEUE_SIZE; index++) queue_[index] = SignalCode::None;
  state_.mode = config_.mode;
  state_.arrowsPerEnd = config_.arrowsPerEnd;
  state_.endNumber = 1;
  state_.periodMs = Rules::periodMs(config_.arrowsPerEnd, perArrowMs());
  state_.remainingMs = state_.periodMs;
  applyLight();
}

uint32_t ShotClock::perArrowMs() const { return Rules::perArrowMs(config_.mode, config_.eventClass); }

uint8_t ShotClock::effectiveArrowsPerEnd() const {
  // Art. 12.5: a shoot-off is a fixed one arrow per athlete whatever the end
  // size of the match was.
  return config_.shootOff ? Rules::shootOffArrows(config_.mode) : config_.arrowsPerEnd;
}

uint32_t ShotClock::totalPeriodMs() const {
  if (config_.mode == Mode::Practice) return config_.practiceMs;
  return Rules::periodMs(effectiveArrowsPerEnd(), perArrowMs());
}

bool ShotClock::sideComplete(uint8_t side) const {
  return state_.sideArrows[side] >= state_.arrowsPerEnd;
}

bool ShotClock::allArrowsShot() const {
  if (Rules::isAlternating(config_.mode)) return sideComplete(0) && sideComplete(1);
  return state_.arrowsShot >= state_.arrowsPerEnd;
}

uint8_t ShotClock::unshotArrows() const {
  if (Rules::isAlternating(config_.mode) && state_.shooter > 0) {
    const uint8_t shot = state_.sideArrows[state_.shooter - 1];
    return shot >= state_.arrowsPerEnd ? 0 : static_cast<uint8_t>(state_.arrowsPerEnd - shot);
  }
  return state_.arrowsShot >= state_.arrowsPerEnd ? 0
                                                  : static_cast<uint8_t>(state_.arrowsPerEnd - state_.arrowsShot);
}

bool ShotClock::clockRunning() const {
  return state_.phase == Phase::Occupy || state_.phase == Phase::Shooting || state_.phase == Phase::Warning;
}

void ShotClock::applyLight() {
  switch (state_.phase) {
    case Phase::Idle:
    case Phase::Break:
      state_.light = Light::Off;
      break;
    case Phase::Shooting: state_.light = Light::Green; break;
    case Phase::Warning: state_.light = Light::Yellow; break;
    default: state_.light = Light::Red; break;
  }
  state_.running = clockRunning();
  state_.finished = state_.phase == Phase::Finished || state_.phase == Phase::Scoring;
}

void ShotClock::emitSignal(uint32_t now, SignalCode code, const char* article) {
  const uint8_t next = static_cast<uint8_t>((queueHead_ + 1) % QUEUE_SIZE);
  if (next == queueTail_) {
    // The sound layer is not keeping up. Say so rather than silently losing a
    // signal athletes are waiting for.
    tracer_.warn(now, "signal_queue_full", name(code));
  } else {
    queue_[queueHead_] = code;
    queueHead_ = next;
  }
  tracer_.signal(now, name(code), signalCount(code), article);
}

bool ShotClock::takeSignal(SignalCode& signal) {
  if (queueTail_ == queueHead_) return false;
  signal = queue_[queueTail_];
  queueTail_ = static_cast<uint8_t>((queueTail_ + 1) % QUEUE_SIZE);
  return true;
}

void ShotClock::rejected(uint32_t now, const char* control) {
  tracer_.warn(now, "control_rejected", control);
}

void ShotClock::enterPhase(uint32_t now, Phase phase, uint32_t durationMs) {
  state_.phase = phase;
  state_.remainingMs = durationMs;
  lastTick_ = now;
  applyLight();
}

void ShotClock::configure(uint32_t now, const SessionConfig& config) {
  config_ = config;

  if (Rules::isTeam(config_.mode)) {
    // Art. 11.1.4.2 fixes the team end at six arrows and the mixed team at
    // four, so an end size that disagrees is corrected rather than obeyed.
    const uint8_t required = Rules::defaultArrowsPerEnd(config_.mode);
    if (config_.arrowsPerEnd != required) {
      tracer_.warn(now, "arrows_per_end", "team end size set by Art. 11.1.4.2");
      config_.arrowsPerEnd = required;
    }
  } else if (config_.mode != Mode::Practice && config_.arrowsPerEnd != Rules::ARROWS_PER_END_SHORT &&
             config_.arrowsPerEnd != Rules::ARROWS_PER_END_LONG) {
    // Art. 10.1 offers three or six "unless specified differently"; anything
    // else is accepted but recorded, so an odd end size in a log is visible.
    tracer_.warn(now, "arrows_per_end", "not 3 or 6 (Art. 10.1)");
  }

  if (config_.firstShooter != 1 && config_.firstShooter != 2) config_.firstShooter = 1;
  if (config_.details < 1) config_.details = 1;
  if (config_.breakAfterEnds > 36) config_.breakAfterEnds = 36;
  if (config_.breakEnabled && config_.breakMs == 0) config_.breakMs = 15 * 60 * 1000;

  state_.mode = config_.mode;
  state_.shootOff = config_.shootOff;
  state_.arrowsPerEnd = effectiveArrowsPerEnd();
  state_.arrowsShot = 0;
  state_.sideArrows[0] = 0;
  state_.sideArrows[1] = 0;
  state_.details = config_.abcdRotation ? config_.details : 1;
  setDetailForThisEnd();
  state_.shooter = Rules::isAlternating(config_.mode) ? config_.firstShooter : 0;

  const uint32_t perArrow = perArrowMs();
  state_.periodMs = totalPeriodMs();

  if (config_.shootOff) {
    const TraceField shootOffFields[] = {{"arrows", state_.arrowsPerEnd}};
    tracer_.rule(now, "12.5", "shoot_off", shootOffFields, 1, "separate one-off end");
  }

  if (config_.mode == Mode::Practice) {
    const TraceField practiceFields[] = {{"duration_ms", static_cast<int32_t>(config_.practiceMs)}};
    // Chapter 14 gives practice a start and a stop and no arrow structure.
    tracer_.rule(now, "14.2", "practice", practiceFields, 1, "director set");
  } else {
    const TraceField fields[] = {
        {"arrows", state_.arrowsPerEnd},
        {"per_arrow_ms", static_cast<int32_t>(perArrow)},
        {"period_ms", static_cast<int32_t>(state_.periodMs)},
    };
    tracer_.rule(now, Rules::perArrowArticle(config_.mode, config_.eventClass), "period", fields, 3,
                 Rules::name(config_.eventClass));
  }

  if (config_.mode == Mode::TeamAlternating) {
    // Art. 11.1.4.3: each team owns an allowance that stops and restarts,
    // rather than getting a fresh clock for every arrow.
    state_.sideRemainingMs[0] = state_.periodMs;
    state_.sideRemainingMs[1] = state_.periodMs;
  }

  enterPhase(now, Phase::Idle, state_.periodMs);
}

void ShotClock::start(uint32_t now) {
  // Art. 11.2.3.2 and 11.1.4.3 describe alternating shooting as one clock
  // stopping and the opponent's starting. That is one action, so it is one
  // control: pressing start while a turn is running hands over.
  if (Rules::isAlternating(config_.mode) && (state_.phase == Phase::Shooting || state_.phase == Phase::Warning)) {
    handoff(now, "director");
    return;
  }

  if (state_.phase != Phase::Idle && state_.phase != Phase::Finished &&
      state_.phase != Phase::Scoring && state_.phase != Phase::Break) {
    rejected(now, "start");
    return;
  }

  if (state_.phase == Phase::Break) {
    leaveBreak(now, true);
    return;
  }

  beginEnd(now);
}

void ShotClock::beginEnd(uint32_t now) {
  state_.arrowsShot = 0;
  state_.sideArrows[0] = 0;
  state_.sideArrows[1] = 0;
  setDetailForThisEnd();
  state_.shooter = Rules::isAlternating(config_.mode) ? config_.firstShooter : 0;
  state_.periodMs = totalPeriodMs();
  pendingShootingMs_ = state_.periodMs;

  if (config_.mode == Mode::TeamAlternating) {
    state_.sideRemainingMs[0] = state_.periodMs;
    state_.sideRemainingMs[1] = state_.periodMs;
  }

  const TraceField fields[] = {{"occupy_ms", static_cast<int32_t>(Rules::OCCUPY_LINE_MS)}};
  tracer_.rule(now, "11.3.1", "occupy_line", fields, 1, nullptr);
  enterPhase(now, Phase::Occupy, Rules::OCCUPY_LINE_MS);
  emitSignal(now, SignalCode::OccupyLine, "11.3.1");
}

void ShotClock::beginShootingPeriod(uint32_t now) {
  if (config_.mode == Mode::Practice) {
    beginShooting(now, config_.practiceMs, "14.2", "practice_period");
    return;
  }
  if (config_.mode == Mode::IndividualAlternating) {
    // Art. 11.2.3.2: twenty seconds to shoot one arrow, fresh each turn.
    beginShooting(now, Rules::PER_ARROW_ALTERNATING_MS, "11.2.3.2", "turn");
    return;
  }
  if (config_.mode == Mode::TeamAlternating) {
    beginShooting(now, state_.sideRemainingMs[state_.shooter - 1], "11.1.4.3", "turn");
    return;
  }
  beginShooting(now, pendingShootingMs_, Rules::perArrowArticle(config_.mode, config_.eventClass), "period");
}

void ShotClock::handoff(uint32_t now, const char* reason) {
  const uint8_t from = state_.shooter;
  if (from != 1 && from != 2) {
    rejected(now, "handoff");
    return;
  }

  if (config_.mode == Mode::TeamAlternating) {
    // Art. 11.1.4.3: "the clock of that team is stopped, displaying the time
    // remaining" - the allowance is banked, not forfeited.
    state_.sideRemainingMs[from - 1] = state_.remainingMs;
  }

  const uint8_t to = from == 1 ? 2 : 1;
  const TraceField fields[] = {
      {"from", from},
      {"to", to},
      {"banked_ms", static_cast<int32_t>(config_.mode == Mode::TeamAlternating ? state_.sideRemainingMs[from - 1] : 0)},
      {"arrows_from", state_.sideArrows[from - 1]},
  };
  tracer_.rule(now, config_.mode == Mode::TeamAlternating ? "11.1.4.3" : "11.2.3.2", "handoff", fields, 4, reason);

  if (allArrowsShot()) {
    finish(now, "all_arrows");
    return;
  }

  state_.shooter = to;
  if (sideComplete(to - 1)) {
    // The other side has already finished its arrows, so the turn goes back.
    state_.shooter = from;
  }

  const uint32_t duration = config_.mode == Mode::TeamAlternating
                                ? state_.sideRemainingMs[state_.shooter - 1]
                                : Rules::PER_ARROW_ALTERNATING_MS;
  if (duration == 0) {
    finish(now, "time_expired");
    return;
  }

  state_.periodMs = duration;
  enterPhase(now, Phase::Shooting, duration);

  // Art. 11.2.3.2 requires a signal when time runs out. Art. 11.3.4 forbids one
  // at the start of each period where several matches share a field, which is
  // why the director-triggered handoff can be made silent.
  const bool timedOut = reason != nullptr && reason[0] == 't';
  if (timedOut || config_.signalEachAlternatingPeriod) emitSignal(now, SignalCode::Start, "11.2.3.2");
}

void ShotClock::finish(uint32_t now, const char* reason) {
  const TraceField fields[] = {
      {"arrows_shot", state_.arrowsShot},
      {"arrows_per_end", state_.arrowsPerEnd},
      {"detail", state_.detail},
  };
  tracer_.rule(now, "11.3.1", "stop", fields, 3, reason);
  enterPhase(now, Phase::Finished, 0);
  emitSignal(now, SignalCode::Stop, "11.3.1");

  // Art. 11.2.3.1: with AB/CD rotation the next detail has ten seconds to take
  // the line, and the whole sequence repeats until every athlete has shot.
  // Odd ends start AB then CD; even ends reverse to CD then AB.
  if (moreDetailsThisEnd()) {
    state_.detail = nextDetailAfter(state_.detail);
    state_.arrowsShot = 0;
    const TraceField rotationFields[] = {
        {"detail", state_.detail},
        {"details", state_.details},
        {"changeover_ms", static_cast<int32_t>(Rules::OCCUPY_LINE_MS)},
    };
    tracer_.rule(now, "11.2.3.1", "detail_changeover", rotationFields, 3, nullptr);
    pendingShootingMs_ = totalPeriodMs();
    state_.periodMs = pendingShootingMs_;
    enterPhase(now, Phase::Occupy, Rules::OCCUPY_LINE_MS);
    emitSignal(now, SignalCode::OccupyLine, "11.3.1");
  }
}

void ShotClock::beginShooting(uint32_t now, uint32_t durationMs, const char* article, const char* what) {
  state_.periodMs = durationMs;
  const TraceField fields[] = {
      {"arrows_unshot", unshotArrows()},
      {"per_arrow_ms", static_cast<int32_t>(perArrowMs())},
      {"period_ms", static_cast<int32_t>(durationMs)},
  };
  tracer_.rule(now, article, what, fields, 3, Rules::name(config_.eventClass));
  enterPhase(now, Phase::Shooting, durationMs);
  emitSignal(now, SignalCode::Start, "11.3.1");
}

void ShotClock::stop(uint32_t now) {
  if (!clockRunning()) {
    rejected(now, "stop");
    return;
  }
  finish(now, "director");
}

void ShotClock::lineClear(uint32_t now) {
  // Art. 11.3.1: the three signals for scoring follow the red light, so the
  // clock must already have stopped. Art. 11.3.2 makes the line being clear the
  // director's judgement, which is why this is a control and not a timeout.
  if (state_.phase != Phase::Finished) {
    rejected(now, "line_clear");
    return;
  }
  const TraceField fields[] = {
      {"arrows_shot", state_.arrowsShot},
      {"arrows_per_end", state_.arrowsPerEnd},
  };
  tracer_.rule(now, "11.3.2", "scoring", fields, 2, nullptr);
  enterPhase(now, Phase::Scoring, 0);
  emitSignal(now, SignalCode::Scoring, "11.3.1");
}

void ShotClock::nextEnd(uint32_t now) {
  if (state_.phase == Phase::Break) {
    leaveBreak(now, false);
    return;
  }
  if (state_.phase != Phase::Scoring && state_.phase != Phase::Finished) {
    rejected(now, "next_end");
    return;
  }
  if (state_.phase == Phase::Scoring && breakDue()) {
    enterBreak(now);
    return;
  }
  state_.endNumber++;
  state_.arrowsShot = 0;
  setDetailForThisEnd();
  state_.periodMs = Rules::periodMs(state_.arrowsPerEnd, perArrowMs());
  enterPhase(now, Phase::Idle, state_.periodMs);
}

void ShotClock::resetEnd(uint32_t now) {
  state_.arrowsShot = 0;
  setDetailForThisEnd();
  state_.periodMs = Rules::periodMs(state_.arrowsPerEnd, perArrowMs());
  enterPhase(now, Phase::Idle, state_.periodMs);
}

void ShotClock::suspend(uint32_t now) {
  if (!clockRunning()) {
    rejected(now, "suspend");
    return;
  }
  const TraceField fields[] = {
      {"clock_ms", static_cast<int32_t>(state_.remainingMs)},
      {"arrows_shot", state_.arrowsShot},
      {"arrows_unshot", unshotArrows()},
  };
  tracer_.rule(now, "11.2.4", "suspend", fields, 3, nullptr);
  enterPhase(now, Phase::Suspended, state_.remainingMs);
}

void ShotClock::resume(uint32_t now) {
  if (state_.phase != Phase::Suspended) {
    rejected(now, "resume");
    return;
  }

  const uint8_t unshot = unshotArrows();
  const uint32_t clockMs = state_.remainingMs;
  const uint32_t perArrow = perArrowMs();
  uint32_t resumeMs = 0;
  const char* article = nullptr;
  const char* reason = nullptr;

  const bool team = config_.mode == Mode::TeamSimultaneous || config_.mode == Mode::TeamAlternating ||
                    config_.mode == Mode::MixedTeam;
  if (team) {
    // Art. 11.2.4.2: keep the clock only if it holds more than 20 s per unshot
    // arrow, otherwise reset to that floor.
    const uint32_t floorMs = Rules::periodMs(unshot, Rules::PER_ARROW_ALTERNATING_MS);
    resumeMs = Rules::teamResumeMs(clockMs, unshot);
    article = "11.2.4.2";
    reason = clockMs > floorMs ? "clock>floor" : "clock<=floor";
    const TraceField fields[] = {
        {"clock_ms", static_cast<int32_t>(clockMs)},
        {"unshot", unshot},
        {"floor_ms", static_cast<int32_t>(floorMs)},
        {"result_ms", static_cast<int32_t>(resumeMs)},
    };
    tracer_.rule(now, article, "resume_recalc", fields, 4, reason);
  } else {
    // Art. 11.2.4.1: a flat per-arrow allowance for the unshot arrows. The
    // article never compares this with the clock, so neither do we.
    resumeMs = Rules::individualResumeMs(unshot, perArrow);
    article = "11.2.4.1";
    const TraceField fields[] = {
        {"clock_ms", static_cast<int32_t>(clockMs)},
        {"unshot", unshot},
        {"per_arrow_ms", static_cast<int32_t>(perArrow)},
        {"result_ms", static_cast<int32_t>(resumeMs)},
    };
    tracer_.rule(now, article, "resume_recalc", fields, 4, "flat_per_arrow");
  }

  pendingShootingMs_ = resumeMs;
  emitSignal(now, SignalCode::Resume, "11.3.3");

  if (config_.replayOccupyOnResume) {
    // Art. 11.2.4 counts the 10-second signal inside the recalculated time.
    const TraceField fields[] = {{"occupy_ms", static_cast<int32_t>(Rules::OCCUPY_LINE_MS)}};
    tracer_.rule(now, "11.2.4", "resume_occupy", fields, 1, "interpretation");
    enterPhase(now, Phase::Occupy, Rules::OCCUPY_LINE_MS);
    emitSignal(now, SignalCode::OccupyLine, "11.3.1");
    return;
  }

  state_.periodMs = resumeMs;
  enterPhase(now, Phase::Shooting, resumeMs);
}

void ShotClock::addArrow(uint32_t now) {
  const uint8_t cap = Rules::isAlternating(config_.mode)
                          ? static_cast<uint8_t>(state_.arrowsPerEnd * 2)
                          : state_.arrowsPerEnd;
  if (state_.arrowsShot >= cap) {
    rejected(now, "add_arrow");
    return;
  }
  state_.arrowsShot++;
  if (Rules::isAlternating(config_.mode) && state_.shooter > 0) state_.sideArrows[state_.shooter - 1]++;
  tracer_.config(now, "arrows_shot", state_.arrowsShot - 1, state_.arrowsShot);
}

void ShotClock::removeArrow(uint32_t now) {
  if (state_.arrowsShot == 0) {
    rejected(now, "remove_arrow");
    return;
  }
  state_.arrowsShot--;
  if (Rules::isAlternating(config_.mode) && state_.shooter > 0 && state_.sideArrows[state_.shooter - 1] > 0) {
    state_.sideArrows[state_.shooter - 1]--;
  }
  tracer_.config(now, "arrows_shot", state_.arrowsShot + 1, state_.arrowsShot);
}

void ShotClock::extendTime(uint32_t now, uint32_t extraMs) {
  if (!clockRunning() && state_.phase != Phase::Suspended) {
    rejected(now, "extend_time");
    return;
  }
  const uint32_t before = state_.remainingMs;
  state_.remainingMs += extraMs;
  state_.periodMs += extraMs;
  const TraceField fields[] = {
      {"before_ms", static_cast<int32_t>(before)},
      {"added_ms", static_cast<int32_t>(extraMs)},
      {"after_ms", static_cast<int32_t>(state_.remainingMs)},
  };
  // Art. 11.2.2 permits an extension "in exceptional circumstances" without
  // bounding it, so the system records the decision rather than limiting it.
  tracer_.rule(now, "11.2.2", "extend_time", fields, 3, "director");
}

void ShotClock::emergency(uint32_t now) {
  const TraceField fields[] = {{"signals", Rules::SIGNALS_EMERGENCY_MINIMUM}};
  tracer_.rule(now, "11.3.3", "emergency", fields, 1, nullptr);
  enterPhase(now, Phase::Emergency, 0);
  emitSignal(now, SignalCode::Emergency, "11.3.3");
}

void ShotClock::clearEmergency(uint32_t now) {
  if (state_.phase != Phase::Emergency) {
    rejected(now, "clear_emergency");
    return;
  }
  state_.arrowsShot = 0;
  setDetailForThisEnd();
  state_.periodMs = Rules::periodMs(state_.arrowsPerEnd, perArrowMs());
  enterPhase(now, Phase::Idle, state_.periodMs);
}

void ShotClock::setDisplayContent(uint32_t now, DisplayContent content) {
  if (state_.display == content) return;
  tracer_.configText(now, "display_content", name(state_.display), name(content));
  state_.display = content;
}

void ShotClock::setMatchTotals(const uint16_t score[2], const uint8_t setPoints[2]) {
  state_.score[0] = score[0];
  state_.score[1] = score[1];
  state_.setPoints[0] = setPoints[0];
  state_.setPoints[1] = setPoints[1];
}

void ShotClock::update(uint32_t now) {
  if (state_.phase == Phase::Break) {
    const uint32_t elapsed = now - lastTick_;
    if (elapsed == 0) return;
    if (elapsed > BACKWARDS_CLOCK_MS) {
      tracer_.warn(now, "clock_backwards", "time went backwards; no time consumed");
      lastTick_ = now;
      return;
    }
    lastTick_ = now;
    state_.remainingMs = elapsed >= state_.remainingMs ? 0 : state_.remainingMs - elapsed;
    if (state_.remainingMs == 0) leaveBreak(now, false);
    return;
  }

  if (!clockRunning()) {
    lastTick_ = now;
    return;
  }

  const uint32_t elapsed = now - lastTick_;
  if (elapsed == 0) return;

  // Unsigned subtraction turns a clock that went backwards into roughly
  // 4.29 billion milliseconds, which consumes any period instantly - the ten
  // second line-occupation phase simply disappears between one line of the
  // loop and the next. A period vanishing without a word is the worst
  // possible failure here, so it is refused and recorded.
  if (elapsed > BACKWARDS_CLOCK_MS) {
    tracer_.warn(now, "clock_backwards", "time went backwards; no time consumed");
    lastTick_ = now;
    return;
  }

  lastTick_ = now;
  state_.remainingMs = elapsed >= state_.remainingMs ? 0 : state_.remainingMs - elapsed;

  if (state_.phase == Phase::Occupy) {
    if (state_.remainingMs == 0) beginShootingPeriod(now);
    return;
  }

  if (state_.phase == Phase::Shooting && Rules::usesWarning(config_.mode) && state_.remainingMs <= Rules::WARNING_MS &&
      state_.remainingMs > 0) {
    const TraceField fields[] = {{"remaining_ms", static_cast<int32_t>(state_.remainingMs)}};
    // Art. 11.3.1: visual only. The article assigns no sound signal to yellow.
    tracer_.rule(now, "11.3.1", "warning", fields, 1, "visual_only");
    state_.phase = Phase::Warning;
    applyLight();
  }

  if (state_.remainingMs == 0) {
    // Art. 11.2.3.2: when the time runs out the opponent's period begins
    // without waiting for the director, so the alternation keeps its rhythm
    // even if a handoff press is missed.
    if (Rules::isAlternating(config_.mode) && !allArrowsShot()) {
      handoff(now, "timeout");
      return;
    }
    finish(now, "timeout");
  }
}

bool ShotClock::breakDue() const {
  if (!config_.breakEnabled || config_.breakAfterEnds == 0) return false;
  return state_.endNumber > 0 && (state_.endNumber % config_.breakAfterEnds) == 0;
}

void ShotClock::enterBreak(uint32_t now) {
  const TraceField fields[] = {
      {"after_end", state_.endNumber},
      {"break_ms", static_cast<int32_t>(config_.breakMs)},
  };
  tracer_.rule(now, "session", "break", fields, 2, "after scoring");
  state_.periodMs = config_.breakMs;
  enterPhase(now, Phase::Break, config_.breakMs);
}

void ShotClock::leaveBreak(uint32_t now, bool startShooting) {
  state_.endNumber++;
  state_.arrowsShot = 0;
  setDetailForThisEnd();
  state_.periodMs = totalPeriodMs();
  if (startShooting) {
    beginEnd(now);
    return;
  }
  enterPhase(now, Phase::Idle, state_.periodMs);
}

uint8_t ShotClock::firstDetailThisEnd() const {
  if (state_.details <= 1) return 1;
  return static_cast<uint8_t>(((state_.endNumber - 1) % state_.details) + 1);
}

uint8_t ShotClock::nextDetailAfter(uint8_t detail) const {
  if (state_.details <= 1) return 1;
  return detail >= state_.details ? 1 : static_cast<uint8_t>(detail + 1);
}

bool ShotClock::moreDetailsThisEnd() const {
  if (!config_.abcdRotation || state_.details <= 1) return false;
  return nextDetailAfter(state_.detail) != firstDetailThisEnd();
}

void ShotClock::setDetailForThisEnd() { state_.detail = firstDetailThisEnd(); }

}  // namespace Core
