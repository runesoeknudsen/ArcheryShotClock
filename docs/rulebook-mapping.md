# Rulebook mapping

Traceability from World Archery Book 3 — Target Archery (version 2026-01-27) to
the code that implements it, the trace record that proves it ran, and the check
that verifies it.

This is an independent, unofficial implementation, not affiliated with or
endorsed by World Archery.

A dash in the last column means the decision is logged but not yet independently
recomputed by `tools/logcheck.py`.

## Chapter 10 — Shooting and conduct

| Article | Requirement | Code | Trace evidence | Checked by |
|---|---|---|---|---|
| 10.1 | Ends of three or six arrows | `Rules::ARROWS_PER_END_SHORT/LONG` | `RULE what=period` | — |

## Chapter 11 — Order of shooting and timing control

| Article | Requirement | Code | Trace evidence | Checked by |
|---|---|---|---|---|
| 11.1.4.1 | Higher-placed athlete chooses who shoots first | `SessionConfig::firstShooter` | `STATE.shooter` | — |
| 11.1.4.2 | Team ends are six arrows, mixed team four | `Rules::defaultArrowsPerEnd` | `WARN arrows_per_end`, `RULE what=period` | `period` |
| 11.1.4.3 | Team alternating banks each team's remaining time | `ShotClock::handoff` | `RULE what=handoff` with `banked_ms` | `alternation` |
| 11.2.1.1 | 30 s per arrow at announced and ranking events | `Rules::perArrowMs` | `RULE art=11.2.1.1` | `period` |
| 11.2.1.2 | 40 s elsewhere, reducible to 30 s | `Rules::perArrowMs` | `RULE art=11.2.1.2` | `period` |
| 11.2.1 | Period is arrows × per-arrow time | `Rules::periodMs` | `RULE what=period` | `period` |
| 11.2.2 | Time may be extended in exceptional circumstances | `ShotClock::extendTime` | `RULE what=extend_time` | `extension`, `clock` |
| 11.2.3.1 | AB/CD rotation, 10 s changeover, starting group rotates each end | `ShotClock::finish` | `RULE what=detail_changeover` | `occupy` |
| 11.2.3.2 | Individual alternating, 20 s per arrow, timeout advances | `ShotClock::handoff` | `RULE art=11.2.3.2` | `alternation` |
| 11.2.4 | Suspension adjusts the time, including the 10 s signal | `ShotClock::suspend/resume` | `RULE what=suspend`, `resume_occupy` | — |
| 11.2.4.1 | Individual resume: flat per-arrow allowance | `Rules::individualResumeMs` | `RULE art=11.2.4.1` | `resume` |
| 11.2.4.2 | Team resume: keep the clock only above the floor | `Rules::teamResumeMs` | `RULE art=11.2.4.2` | `resume` |
| 11.3.1 | RED, 10 s, GREEN; yellow at 30 s; RED and stop | `ShotClock`, `Phase` | `STATE.phase` + `STATE.light` | `lights`, `occupy`, `warning-signal` |
| 11.3.1 | Signal counts 2 / 1 / 2 / 3 | `Rules::SIGNALS_*` | `SIGNAL.code` + `beeps` | `signals` |
| 11.3.1 | The digital clock takes precedence over the lights | light derived from phase and clock | `RENDER.text` vs `STATE.rem_ms` | `panel` |
| 11.3.2 | Scoring signalled when the line is clear | `ShotClock::lineClear` | `RULE art=11.3.2` | — |
| 11.3.3 | Emergency: at least five sound signals | `EmergencyStop`, `ShotClock::emergency` | `SIGNAL code=EMERGENCY` | `signals` |
| 11.3.3 | One sound signal to continue after suspension | `SignalCode::Resume` | `SIGNAL code=RESUME` | `signals` |
| 11.3.4 | No period signal when matches share a field | `SessionConfig::signalEachAlternatingPeriod` | absence of `SIGNAL` on a director handoff | — |

## Chapter 12 — Scoring

| Article | Requirement | Code | Trace evidence | Checked by |
|---|---|---|---|---|
| 12.1.3 | Values entered in descending order as called | `MatchLogic::recordArrow` | `RULE art=12.1.3` | — |
| 12.1.4 | Recurve and barebow set play, compound cumulative | `Core::defaultScoring` | `RULE art=12.1.4` | — |
| 12.1.4.1 | Individual set play, first to six set points | `MatchLogic::completeEnd` | `RULE art=12.1.4.1` | `set-points` |
| 12.1.4.2 | Team set play, first to five set points | `MatchLogic::completeEnd` | `RULE art=12.1.4.2` | `set-points` |
| 12.1.4.3 | Individual cumulative over five ends | `MatchLogic::completeEnd` | `RULE art=12.1.4.3` | — |
| 12.1.4.4 | Team cumulative over four ends | `MatchLogic::completeEnd` | `RULE art=12.1.4.4` | — |
| 12.2.2 | Surplus arrows score the lowest values | refused at entry | `WARN arrow_count` | — |
| 12.2.2.3 | Unshot team arrows score as a miss | `MatchLogic::fillUnshotArrows` | `RULE art=12.2.2.3` | — |
| 12.2.8 | A miss is recorded as M | `Core::ARROW_MISS` | `RULE what=arrow reason=M` | — |
| 12.5 | Shoot-off is a separate one-off end | `SessionConfig::shootOff` | `RULE art=12.5` | — |

## Chapter 13 — Consequences of breaking rules

| Article | Requirement | Code | Trace evidence | Checked by |
|---|---|---|---|---|
| 13.3 | Lose the highest-scoring arrow of the end | `MatchLogic::forfeitHighest` | `RULE art=13.3` | `forfeit` |
| 13.4 | One warning, then disqualification | `MatchLogic::warn` | `RULE art=13.4` | — |
| 13.5 | Disqualification without warning | `MatchLogic::disqualify` | `RULE art=13.5` | — |
| 13.6.1 | Yellow card: restart behind the 1 m line | `MatchLogic::yellowCard` | `RULE art=13.6.1` | — |
| 13.6.2 | Card ignored: lose the highest-scoring arrow | `MatchLogic::forfeitHighest` | `RULE art=13.6.2` | `forfeit` |

## Chapter 14 — Practice

| Article | Requirement | Code | Trace evidence | Checked by |
|---|---|---|---|---|
| 14.2 | Director signals start and stop, no arrow structure | `Mode::Practice` | `RULE art=14.2` | — |

## Where the book is silent

These are project decisions, not rules, and are marked as such in the code:

- **The sound itself.** Article 11.3 gives signal counts and nothing else — no
  device, tone, pitch, volume, beep length or gap. Book 3 contains no
  occurrence of whistle, horn, buzzer, bell, decibel or hertz. Beep and gap are
  configurable, with defaults chosen for the field.
- **Resuming after a suspension.** Art. 11.2.4 counts the 10 s signal inside the
  recalculated time; Art. 11.3.3 gives one sound signal to continue. The book
  never says whether the two-signal line-occupation sequence is re-run.
  Replaying it is the default and is switchable; the `RULE` record marks it as
  an interpretation.
- **Arrow values.** Book 3 never enumerates them or states what X scores; target
  faces live in Book 2. The 0–10 plus X range here is a project decision.
- **A level cumulative match.** Art. 12.1.4.3/.4 say only that the highest total
  wins. No tie-break is given, so the system reports a draw.
- **Set-play match length.** Book 3 gives a winning threshold and no maximum
  number of sets, so none is assumed.
- **The idle colour.** Article 11.3.1 names red, green and yellow and says
  nothing about what a panel shows when no shooting period is running. White is
  used, chosen so it cannot be mistaken at distance for the warning yellow.
- **Round composition.** Number of ends, arrows and distances for a
  qualification round are in Book 2, not Book 3, and are not hardcoded here.
