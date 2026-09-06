#!/usr/bin/env python3
"""Check a captured trace log against the rules the firmware claims to follow.

The firmware emits one NDJSON record per line (see docs/trace-format.md). This
script reads such a capture - from `pio device monitor > session.log`, or from
the native tests - and re-derives the decisions the firmware logged, so it can
disagree with them.

That is the point: a RULE record carries the inputs a decision was made from,
not only its result, so compliance can be checked independently rather than
taken on the firmware's word.

Usage:
    pio device monitor -b 921600 > session.log
    python3 tools/logcheck.py session.log
    python3 tools/logcheck.py --selftest

Exit status is 0 when no errors were found, 1 otherwise. Warnings do not fail
the run unless --strict is given.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from typing import Any, Iterable, Sequence

SUPPORTED_SCHEMA = 3

ERROR = "error"
WARNING = "warning"
INFO = "info"

# Sound-signal counts that World Archery Book 3 Article 11.3 prescribes. These
# counts are load-bearing: officials on the field are trained to recognise them,
# so a wrong count is a compliance failure, not a cosmetic one.
WA_SIGNAL_BEEPS = {
    "OCCUPY_LINE": 2,
    "START": 1,
    "STOP": 2,
    "SCORING": 3,
    "RESUME": 1,
}
WA_EMERGENCY_MINIMUM_BEEPS = 5

# Light colour is derived from the clock (Art. 11.3.1 makes the digital clock
# authoritative), so these pairings must always hold.
EXPECTED_LIGHT = {
    "SHOOTING": "GREEN",
    "WARNING": "YELLOW",
    "FINISHED": "RED",
    "SCORING": "RED",
    "OCCUPY": "RED",
    "IDLE": "OFF",
    "BREAK": "OFF",
}

ALTERNATING_MODES = {"IND_ALT", "TEAM_ALT"}
TEAM_MODES = {"TEAM_SIMUL", "TEAM_ALT", "MIXED_TEAM"}

# Art. 11.2.1.1 and 11.2.1.2. The event class only changes the individual
# non-alternating rate; everything else is 20 s per arrow in both classes.
PER_ARROW_MS = {
    ("IND_NONALT", "ANNOUNCED"): 30000,
    ("IND_NONALT", "OTHER"): 40000,
    ("IND_NONALT", "OTHER_REDUCED"): 30000,
}
PER_ARROW_ALTERNATING_MS = 20000

# Art. 12.1.4.1 / 12.1.4.2.
SET_POINTS_TO_WIN = {"12.1.4.1": 6, "12.1.4.2": 5}
SET_POINTS_WIN = 2
SET_POINTS_TIE = 1
# The 30 s warning does not apply where periods are only 20 s long.
YELLOW_WARNING_MS = 30000
OCCUPY_PERIOD_MS = 10000
# Longest acceptable silence while the clock runs. The heartbeat is 1 Hz, so a
# gap much beyond that means the main loop stalled - which would show up as
# clock jitter on the field.
MAX_QUIET_MS = 2500

# Below roughly this much free heap the Wi-Fi access point starts refusing
# connections and dropping clients, which presents as a flaky network rather
# than as a memory problem. Reported so it is recognisable.
LOW_HEAP_BYTES = 24000
LOW_HEAP_BLOCK_BYTES = 8000


@dataclass
class Finding:
    severity: str
    check: str
    message: str
    line: int | None = None
    seq: int | None = None

    def render(self) -> str:
        where = f"line {self.line}" if self.line is not None else "-"
        if self.seq is not None:
            where += f", seq {self.seq}"
        return f"[{self.severity.upper():7}] {self.check}: {self.message} ({where})"


@dataclass
class Capture:
    records: list[dict[str, Any]] = field(default_factory=list)
    line_numbers: list[int] = field(default_factory=list)
    skipped_lines: int = 0
    malformed_lines: int = 0

    def of_kind(self, kind: str) -> list[tuple[int, dict[str, Any]]]:
        return [
            (self.line_numbers[index], record)
            for index, record in enumerate(self.records)
            if record.get("r") == kind
        ]

    def numbered(self) -> Iterable[tuple[int, dict[str, Any]]]:
        return zip(self.line_numbers, self.records)


def parse(lines: Iterable[str]) -> Capture:
    """Read a capture, tolerating the free-form lines the ESP-IDF stack prints.

    With CORE_DEBUG_LEVEL raised, bluedroid and the Arduino core write plain
    text to the same UART. Those lines are counted and skipped rather than
    treated as failures - but a line that looks like JSON and is not gets
    reported, because that would mean the tracer itself is producing bad output.
    """
    capture = Capture()
    for number, raw in enumerate(lines, start=1):
        text = raw.strip()
        if not text:
            continue
        if not text.startswith("{"):
            capture.skipped_lines += 1
            continue
        try:
            record = json.loads(text)
        except json.JSONDecodeError:
            capture.malformed_lines += 1
            continue
        if not isinstance(record, dict):
            capture.malformed_lines += 1
            continue
        capture.records.append(record)
        capture.line_numbers.append(number)
    return capture


def check_envelope(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    for line, record in capture.numbered():
        for key in ("t", "seq", "r"):
            if key not in record:
                findings.append(
                    Finding(ERROR, "envelope", f"record is missing required field {key!r}", line)
                )
        if record.get("trunc"):
            findings.append(
                Finding(
                    WARNING,
                    "envelope",
                    "record was truncated; some fields are missing from the log",
                    line,
                    record.get("seq"),
                )
            )
    if capture.malformed_lines:
        findings.append(
            Finding(
                ERROR,
                "envelope",
                f"{capture.malformed_lines} line(s) started as JSON but did not parse",
            )
        )
    return findings


def check_schema(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    for line, record in capture.numbered():
        schema = record.get("schema")
        if schema is None:
            continue
        if schema != SUPPORTED_SCHEMA:
            findings.append(
                Finding(
                    ERROR,
                    "schema",
                    f"record uses schema {schema}, this checker understands {SUPPORTED_SCHEMA}",
                    line,
                    record.get("seq"),
                )
            )
    return findings


def check_sequence(capture: Capture) -> list[Finding]:
    """Sequence numbers increment by exactly one, so gaps are provable.

    A gap means records were lost. The firmware counts its own drops and emits
    a WARN with the count, so a gap that the firmware admitted to is reported as
    a warning, and a gap it did not is an error - that is lost evidence with no
    explanation, which could be a stalled UART or a truncated capture.
    """
    findings: list[Finding] = []
    admitted = 0
    for _, record in capture.numbered():
        if record.get("r") == "WARN" and record.get("code") == "trace_dropped":
            admitted += record.get("count", 0)

    expected: int | None = None
    unexplained = 0
    for line, record in capture.numbered():
        seq = record.get("seq")
        if not isinstance(seq, int):
            continue
        if expected is None:
            if seq != 1:
                findings.append(
                    Finding(
                        WARNING,
                        "sequence",
                        f"capture starts at seq {seq}; the beginning of the session is missing",
                        line,
                        seq,
                    )
                )
        elif seq != expected:
            missing = seq - expected
            unexplained += max(missing, 0)
            findings.append(
                Finding(
                    WARNING if missing > 0 else ERROR,
                    "sequence",
                    f"expected seq {expected}, found {seq}"
                    + (f" ({missing} record(s) missing)" if missing > 0 else " (out of order)"),
                    line,
                    seq,
                )
            )
        expected = seq + 1

    if unexplained > admitted:
        findings.append(
            Finding(
                ERROR,
                "sequence",
                f"{unexplained} record(s) missing but the firmware only admitted dropping {admitted}",
            )
        )
    return findings


def check_timestamps(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    previous: int | None = None
    for line, record in capture.numbered():
        now = record.get("t")
        if not isinstance(now, int):
            continue
        if previous is not None and now < previous:
            # millis() wraps after roughly 49.7 days; anything else is a fault.
            if previous - now < 0xFFFFFFFF - 60000:
                findings.append(
                    Finding(
                        ERROR,
                        "timestamps",
                        f"time went backwards, {previous} -> {now}",
                        line,
                        record.get("seq"),
                    )
                )
        previous = now
    return findings


def check_boot(capture: Capture) -> list[Finding]:
    boots = capture.of_kind("BOOT")
    if not boots:
        return [
            Finding(
                WARNING,
                "boot",
                "no BOOT record; the capture does not say which firmware produced it",
            )
        ]
    findings = []
    if capture.records and capture.records[0].get("r") != "BOOT":
        findings.append(
            Finding(INFO, "boot", "capture does not start at boot; earlier records are missing")
        )
    return findings


def check_state_consistency(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    for line, record in capture.of_kind("STATE"):
        seq = record.get("seq")
        phase = record.get("phase")
        light = record.get("light")
        remaining = record.get("rem_ms", 0)

        if record.get("fin") and remaining != 0:
            findings.append(
                Finding(ERROR, "state", f"finished but {remaining} ms still on the clock", line, seq)
            )
        if record.get("run") and record.get("fin"):
            findings.append(Finding(ERROR, "state", "running and finished at the same time", line, seq))

        expected_light = EXPECTED_LIGHT.get(phase)
        if expected_light and light != expected_light:
            findings.append(
                Finding(
                    ERROR,
                    "lights",
                    f"phase {phase} should show {expected_light}, log says {light} (Art. 11.3.1)",
                    line,
                    seq,
                )
            )

        period = record.get("per_ms", 0)
        if period and remaining > period:
            findings.append(
                Finding(
                    ERROR,
                    "state",
                    f"{remaining} ms remaining exceeds the {period} ms period",
                    line,
                    seq,
                )
            )
    return findings


def check_clock_monotonic(capture: Capture) -> list[Finding]:
    """While a period runs, the clock only ever counts down.

    It may jump up on a start, reset, duration change or an Art. 11.2.2 time
    extension - each of which leaves its own record - so those are the only
    permitted increases.
    """
    findings: list[Finding] = []
    allowed_increase = True
    previous: dict[str, Any] | None = None

    for line, record in capture.numbered():
        kind = record.get("r")
        if kind in {"INPUT", "CFG", "BOOT"} or (kind == "RULE" and record.get("what") == "extend_time"):
            allowed_increase = True
            continue
        if kind != "STATE":
            continue
        if previous is not None and record.get("rem_ms", 0) > previous.get("rem_ms", 0):
            if not allowed_increase:
                findings.append(
                    Finding(
                        ERROR,
                        "clock",
                        f"clock rose from {previous.get('rem_ms')} to {record.get('rem_ms')} ms "
                        "with no start, reset, configuration change or time extension to explain it",
                        line,
                        record.get("seq"),
                    )
                )
        if record.get("cause") not in {"boot", "start"}:
            allowed_increase = False
        previous = record
    return findings


def check_quiet_periods(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    previous_time: int | None = None
    running = False
    for line, record in capture.numbered():
        now = record.get("t")
        if not isinstance(now, int):
            continue
        if running and previous_time is not None and now - previous_time > MAX_QUIET_MS:
            findings.append(
                Finding(
                    WARNING,
                    "liveness",
                    f"{now - previous_time} ms with no record while the clock was running; "
                    "the main loop may have stalled",
                    line,
                    record.get("seq"),
                )
            )
        if record.get("r") == "STATE":
            running = bool(record.get("run"))
        previous_time = now
    return findings


def check_signals(capture: Capture) -> list[Finding]:
    """Signal counts must match Article 11.3 exactly.

    Codes outside the World Archery vocabulary are tolerated only in PLAIN
    mode, which is the pre-rulebook club timer this firmware started as.
    """
    findings: list[Finding] = []
    mode = "PLAIN"
    for line, record in capture.numbered():
        if record.get("r") == "STATE":
            mode = record.get("mode", mode)
            continue
        if record.get("r") != "SIGNAL":
            continue

        code = record.get("code")
        beeps = record.get("beeps")
        seq = record.get("seq")

        if code == "EMERGENCY":
            if not isinstance(beeps, int) or beeps < WA_EMERGENCY_MINIMUM_BEEPS:
                findings.append(
                    Finding(
                        ERROR,
                        "signals",
                        f"emergency signal gave {beeps} sounds, Art. 11.3.3 requires at least "
                        f"{WA_EMERGENCY_MINIMUM_BEEPS}",
                        line,
                        seq,
                    )
                )
            continue

        expected = WA_SIGNAL_BEEPS.get(code)
        if expected is None:
            severity = INFO if mode == "PLAIN" else ERROR
            findings.append(
                Finding(
                    severity,
                    "signals",
                    f"signal {code!r} is not part of the Article 11.3 vocabulary"
                    + (" (accepted in PLAIN mode)" if severity == INFO else ""),
                    line,
                    seq,
                )
            )
            continue
        if beeps != expected:
            findings.append(
                Finding(
                    ERROR,
                    "signals",
                    f"signal {code} gave {beeps} sounds, Art. 11.3 requires {expected}",
                    line,
                    seq,
                )
            )
    return findings


def check_yellow_warning(capture: Capture) -> list[Finding]:
    """YELLOW at 30 s remaining, and never in an alternating mode."""
    findings: list[Finding] = []
    for line, record in capture.of_kind("STATE"):
        if record.get("phase") != "WARNING":
            continue
        mode = record.get("mode")
        seq = record.get("seq")
        if mode in ALTERNATING_MODES:
            findings.append(
                Finding(
                    ERROR,
                    "warning-signal",
                    f"YELLOW shown in {mode}; the 30 s warning is not used in alternating shooting",
                    line,
                    seq,
                )
            )
            continue
        remaining = record.get("rem_ms", 0)
        if remaining > YELLOW_WARNING_MS:
            findings.append(
                Finding(
                    ERROR,
                    "warning-signal",
                    f"YELLOW shown with {remaining} ms left; the warning belongs at "
                    f"{YELLOW_WARNING_MS} ms",
                    line,
                    seq,
                )
            )
    return findings


def check_occupy_period(capture: Capture) -> list[Finding]:
    """GREEN is only ever entered from the 10 s line-occupation period.

    Art. 11.3.1: RED and two sounds to occupy the line, then 10 s later GREEN
    and one sound. Shooting that starts without that lead-in is a rules
    failure, not a timing detail.
    """
    findings: list[Finding] = []
    previous_phase: str | None = None
    occupy_started: int | None = None

    for line, record in capture.of_kind("STATE"):
        phase = record.get("phase")
        mode = record.get("mode")
        now = record.get("t")
        seq = record.get("seq")

        if phase == "OCCUPY" and previous_phase != "OCCUPY":
            occupy_started = now
        if (
            phase == "SHOOTING"
            and previous_phase is not None
            and previous_phase not in {"SHOOTING", "OCCUPY", "SUSPENDED"}
        ):
            # previous_phase is None only for the first state in the capture,
            # which may well begin mid-session; that is not evidence of a fault.
            if mode != "PLAIN":
                findings.append(
                    Finding(
                        ERROR,
                        "occupy",
                        f"shooting began from {previous_phase} with no line-occupation period "
                        "(Art. 11.3.1)",
                        line,
                        seq,
                    )
                )
        if phase == "SHOOTING" and previous_phase == "OCCUPY" and occupy_started is not None:
            elapsed = now - occupy_started
            # A period that ends in the same millisecond it began is the
            # signature of a clock that ran backwards, not a short countdown.
            if elapsed < 500:
                findings.append(
                    Finding(
                        ERROR,
                        "occupy",
                        f"the line-occupation period lasted {elapsed} ms - it did not run at all",
                        line,
                        seq,
                    )
                )
            elif abs(elapsed - OCCUPY_PERIOD_MS) > 500:
                findings.append(
                    Finding(
                        ERROR,
                        "occupy",
                        f"line-occupation period lasted {elapsed} ms, Art. 11.3.1 requires "
                        f"{OCCUPY_PERIOD_MS} ms",
                        line,
                        seq,
                    )
                )
        previous_phase = phase
    return findings


def check_period_calculation(capture: Capture) -> list[Finding]:
    """Recompute the period from the arrows and the per-arrow rate.

    Art. 11.2.1: "The total time allowed to shoot an end will be determined by
    the total number of arrows to shoot in the end." The RULE record carries
    both inputs and the result, so the multiplication can be checked rather
    than believed - and the rate itself is checked against the article the
    firmware cited and the event class it named.
    """
    findings: list[Finding] = []
    mode = "IND_NONALT"
    for line, record in capture.numbered():
        if record.get("r") == "STATE":
            mode = record.get("mode", mode)
            continue
        if record.get("r") != "RULE" or record.get("what") not in {"period", "turn"}:
            continue

        seq = record.get("seq")
        arrows = record.get("arrows", record.get("arrows_unshot"))
        per_arrow = record.get("per_arrow_ms")
        period = record.get("period_ms")

        if record.get("what") == "period" and None not in (arrows, per_arrow, period):
            if arrows * per_arrow != period:
                findings.append(
                    Finding(
                        ERROR,
                        "period",
                        f"{arrows} arrows at {per_arrow} ms should be {arrows * per_arrow} ms, "
                        f"log says {period} ms (Art. 11.2.1)",
                        line,
                        seq,
                    )
                )

        event_class = record.get("reason")
        if per_arrow is not None and event_class in {"ANNOUNCED", "OTHER", "OTHER_REDUCED"}:
            expected = (
                PER_ARROW_ALTERNATING_MS
                if mode in ALTERNATING_MODES or mode in TEAM_MODES
                else PER_ARROW_MS.get((mode, event_class))
            )
            if expected is not None and per_arrow != expected:
                findings.append(
                    Finding(
                        ERROR,
                        "period",
                        f"{mode} at event class {event_class} allows {expected} ms per arrow, "
                        f"log says {per_arrow} ms (Art. {record.get('art')})",
                        line,
                        seq,
                    )
                )
    return findings


def check_resume_recalculation(capture: Capture) -> list[Finding]:
    """Re-derive the Art. 11.2.4 recalculation independently.

    The individual rule and the team rule are structurally different, and the
    firmware states which one it applied, so each is recomputed against its own
    article rather than against a single shared formula.
    """
    findings: list[Finding] = []
    for line, record in capture.of_kind("RULE"):
        if record.get("what") != "resume_recalc":
            continue
        seq = record.get("seq")
        article = record.get("art")
        unshot = record.get("unshot")
        result = record.get("result_ms")

        if article == "11.2.4.1":
            per_arrow = record.get("per_arrow_ms")
            if None in (unshot, per_arrow, result):
                continue
            expected = unshot * per_arrow
            if result != expected:
                findings.append(
                    Finding(
                        ERROR,
                        "resume",
                        f"Art. 11.2.4.1 gives {unshot} unshot arrows {per_arrow} ms each, "
                        f"so {expected} ms, but the log says {result} ms",
                        line,
                        seq,
                    )
                )
            if record.get("clock_ms") is not None and result == record.get("clock_ms") and expected != result:
                findings.append(
                    Finding(
                        ERROR,
                        "resume",
                        "Art. 11.2.4.1 never compares against the clock, but the result matches it",
                        line,
                        seq,
                    )
                )
        elif article == "11.2.4.2":
            clock = record.get("clock_ms")
            if None in (unshot, clock, result):
                continue
            floor = unshot * PER_ARROW_ALTERNATING_MS
            expected = clock if clock > floor else floor
            if result != expected:
                findings.append(
                    Finding(
                        ERROR,
                        "resume",
                        f"Art. 11.2.4.2 keeps the clock only above the {floor} ms floor, "
                        f"so {expected} ms, but the log says {result} ms",
                        line,
                        seq,
                    )
                )
    return findings


def check_extensions(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    for line, record in capture.of_kind("RULE"):
        if record.get("what") != "extend_time":
            continue
        before, added, after = record.get("before_ms"), record.get("added_ms"), record.get("after_ms")
        if None in (before, added, after):
            continue
        if before + added != after:
            findings.append(
                Finding(
                    ERROR,
                    "extension",
                    f"{before} ms plus {added} ms should be {before + added} ms, log says {after} ms",
                    line,
                    record.get("seq"),
                )
            )
    return findings


def check_alternation(capture: Capture) -> list[Finding]:
    """Alternating shooting must actually alternate (Art. 11.1.4.3)."""
    findings: list[Finding] = []
    previous_to: int | None = None
    for line, record in capture.of_kind("RULE"):
        if record.get("what") != "handoff":
            continue
        side_from, side_to = record.get("from"), record.get("to")
        seq = record.get("seq")
        if side_from == side_to:
            findings.append(Finding(ERROR, "alternation", "a handoff to the same side", line, seq))
        if previous_to is not None and side_from != previous_to:
            findings.append(
                Finding(
                    ERROR,
                    "alternation",
                    f"the previous handoff gave the turn to side {previous_to}, "
                    f"but this one takes it from side {side_from}",
                    line,
                    seq,
                )
            )
        previous_to = side_to
    return findings


def check_set_points(capture: Capture) -> list[Finding]:
    """Recompute set points from the end totals (Art. 12.1.4.1 / 12.1.4.2)."""
    findings: list[Finding] = []
    running = [0, 0]
    for line, record in capture.of_kind("RULE"):
        if record.get("what") != "set_points":
            continue
        seq = record.get("seq")
        total_a, total_b = record.get("total_a"), record.get("total_b")
        awarded = (record.get("awarded_a"), record.get("awarded_b"))
        if None in (total_a, total_b, *awarded):
            continue

        if total_a > total_b:
            expected = (SET_POINTS_WIN, 0)
        elif total_b > total_a:
            expected = (0, SET_POINTS_WIN)
        else:
            expected = (SET_POINTS_TIE, SET_POINTS_TIE)

        if awarded != expected:
            findings.append(
                Finding(
                    ERROR,
                    "set-points",
                    f"{total_a} against {total_b} should award {expected}, log says {awarded} "
                    f"(Art. {record.get('art')})",
                    line,
                    seq,
                )
            )

        running[0] += awarded[0]
        running[1] += awarded[1]
        logged = (record.get("set_points_a"), record.get("set_points_b"))
        if logged != (running[0], running[1]):
            findings.append(
                Finding(
                    ERROR,
                    "set-points",
                    f"set points should now be {tuple(running)}, log says {logged}",
                    line,
                    seq,
                )
            )
    return findings


def check_forfeits(capture: Capture) -> list[Finding]:
    """Art. 13.3: the highest-scoring arrow of the end is the one lost."""
    findings: list[Finding] = []
    for line, record in capture.of_kind("RULE"):
        if record.get("what") != "forfeit_highest":
            continue
        before, after, lost = record.get("before"), record.get("after"), record.get("lost_value")
        if None in (before, after, lost):
            continue
        if before - lost != after:
            findings.append(
                Finding(
                    ERROR,
                    "forfeit",
                    f"losing {lost} from {before} should leave {before - lost}, log says {after} "
                    f"(Art. {record.get('art')})",
                    line,
                    record.get("seq"),
                )
            )
    return findings


def check_panel(capture: Capture) -> list[Finding]:
    """The panel must agree with the clock it is supposed to be showing.

    Art. 11.3.1 makes the digital clock authoritative, so a RENDER record whose
    text disagrees with the state that produced it means athletes are reading
    something the firmware does not believe.
    """
    findings: list[Finding] = []
    remaining: int | None = None
    for line, record in capture.numbered():
        if record.get("r") == "STATE":
            remaining = record.get("rem_ms")
            continue
        if record.get("r") != "RENDER" or record.get("content") != "CLOCK" or remaining is None:
            continue

        text = record.get("text", "")
        if ":" not in text:
            continue
        try:
            minutes, seconds = (int(part) for part in text.split(":"))
        except ValueError:
            findings.append(Finding(ERROR, "panel", f"unreadable clock text {text!r}", line, record.get("seq")))
            continue

        # The panel rounds up, so any part of a second still reads as that
        # second and zero appears only when the period is genuinely over.
        expected = -(-remaining // 1000)
        shown = minutes * 60 + seconds
        if shown != expected % 6000:
            findings.append(
                Finding(
                    ERROR,
                    "panel",
                    f"panel reads {text} but the clock holds {remaining} ms",
                    line,
                    record.get("seq"),
                )
            )
    return findings


def check_heap(capture: Capture) -> list[Finding]:
    """Watch the board's memory, because a starved heap looks like a bad network.

    A failing access point and a fragmented heap produce the same symptoms from
    the outside. The HEALTH record makes the difference visible.
    """
    findings: list[Finding] = []
    worst_free: int | None = None
    worst_block: int | None = None
    worst_line = None
    worst_seq = None

    for line, record in capture.of_kind("HEALTH"):
        free = record.get("heap_min", record.get("heap"))
        block = record.get("heap_block")
        if free is not None and (worst_free is None or free < worst_free):
            worst_free, worst_line, worst_seq = free, line, record.get("seq")
        if block is not None and (worst_block is None or block < worst_block):
            worst_block = block

    if worst_free is not None and worst_free < LOW_HEAP_BYTES:
        findings.append(
            Finding(
                WARNING,
                "heap",
                f"free heap fell to {worst_free} bytes; below about {LOW_HEAP_BYTES} the access "
                "point becomes unreliable",
                worst_line,
                worst_seq,
            )
        )
    if worst_block is not None and worst_block < LOW_HEAP_BLOCK_BYTES:
        findings.append(
            Finding(
                WARNING,
                "heap",
                f"largest free block fell to {worst_block} bytes; the heap is fragmented even if "
                "the total looks healthy",
                worst_line,
                worst_seq,
            )
        )
    return findings


def check_faults(capture: Capture) -> list[Finding]:
    findings = []
    for line, record in capture.of_kind("ERR"):
        findings.append(
            Finding(
                ERROR,
                "faults",
                f"firmware reported an error: {record.get('code')} - {record.get('detail')}",
                line,
                record.get("seq"),
            )
        )
    for line, record in capture.of_kind("WARN"):
        findings.append(
            Finding(
                WARNING,
                "faults",
                f"firmware reported a warning: {record.get('code')}",
                line,
                record.get("seq"),
            )
        )
    return findings


CHECKS = (
    check_envelope,
    check_schema,
    check_sequence,
    check_timestamps,
    check_boot,
    check_state_consistency,
    check_clock_monotonic,
    check_quiet_periods,
    check_signals,
    check_yellow_warning,
    check_occupy_period,
    check_period_calculation,
    check_resume_recalculation,
    check_extensions,
    check_alternation,
    check_set_points,
    check_forfeits,
    check_panel,
    check_heap,
    check_faults,
)


def run_checks(capture: Capture) -> list[Finding]:
    findings: list[Finding] = []
    for check in CHECKS:
        findings.extend(check(capture))
    return findings


def summarise(capture: Capture, findings: Sequence[Finding]) -> dict[str, Any]:
    counts = {ERROR: 0, WARNING: 0, INFO: 0}
    for finding in findings:
        counts[finding.severity] = counts.get(finding.severity, 0) + 1
    kinds: dict[str, int] = {}
    for record in capture.records:
        kind = record.get("r", "?")
        kinds[kind] = kinds.get(kind, 0) + 1
    return {
        "records": len(capture.records),
        "record_kinds": kinds,
        "skipped_lines": capture.skipped_lines,
        "malformed_lines": capture.malformed_lines,
        "errors": counts[ERROR],
        "warnings": counts[WARNING],
        "info": counts[INFO],
    }


GOOD_LOG = """\
{"t":10,"seq":1,"r":"BOOT","fw":"selftest","schema":3,"level":"MINIMAL"}
{"t":12,"seq":2,"r":"RULE","art":"11.2.1.1","what":"period","arrows":3,"per_arrow_ms":30000,"period_ms":90000,"reason":"ANNOUNCED"}
{"t":20,"seq":3,"r":"INPUT","src":"web","ctl":"start","hold_ms":0}
{"t":20,"seq":4,"r":"STATE","cause":"start","schema":3,"phase":"OCCUPY","light":"RED","mode":"IND_NONALT","disp":"CLOCK","rem_ms":10000,"per_ms":90000,"end":1,"set":0,"arrows":0,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":true,"fin":false,"snd":true,"bt":false,"bri":16}
{"t":20,"seq":5,"r":"SIGNAL","code":"OCCUPY_LINE","beeps":2,"art":"11.3.1"}
{"t":20,"seq":6,"r":"RENDER","content":"CLOCK","light":"RED","text":"00:10","crc":111,"lit":40}
{"t":10020,"seq":7,"r":"STATE","cause":"tick","schema":3,"phase":"SHOOTING","light":"GREEN","mode":"IND_NONALT","disp":"CLOCK","rem_ms":90000,"per_ms":90000,"end":1,"set":0,"arrows":0,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":true,"fin":false,"snd":true,"bt":false,"bri":16}
{"t":10020,"seq":8,"r":"SIGNAL","code":"START","beeps":1,"art":"11.3.1"}
{"t":10020,"seq":9,"r":"RENDER","content":"CLOCK","light":"GREEN","text":"01:30","crc":222,"lit":60}
{"t":70020,"seq":10,"r":"STATE","cause":"tick","schema":3,"phase":"WARNING","light":"YELLOW","mode":"IND_NONALT","disp":"CLOCK","rem_ms":30000,"per_ms":90000,"end":1,"set":0,"arrows":2,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":true,"fin":false,"snd":true,"bt":false,"bri":16}
{"t":100020,"seq":11,"r":"STATE","cause":"finish","schema":3,"phase":"FINISHED","light":"RED","mode":"IND_NONALT","disp":"CLOCK","rem_ms":0,"per_ms":90000,"end":1,"set":0,"arrows":3,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":false,"fin":true,"snd":true,"bt":false,"bri":16}
{"t":100020,"seq":12,"r":"SIGNAL","code":"STOP","beeps":2,"art":"11.3.1"}
{"t":100520,"seq":13,"r":"SIGNAL","code":"SCORING","beeps":3,"art":"11.3.1"}
{"t":101000,"seq":14,"r":"RULE","art":"12.1.4.1","what":"set_points","end":1,"total_a":27,"total_b":25,"awarded_a":2,"awarded_b":0,"set_points_a":2,"set_points_b":0}
{"t":101100,"seq":15,"r":"RULE","art":"13.3","what":"forfeit_highest","side":0,"arrow_index":0,"lost_value":10,"before":27,"after":17,"reason":"judge decision"}
{"t":101200,"seq":16,"r":"RULE","art":"11.2.2","what":"extend_time","before_ms":30000,"added_ms":15000,"after_ms":45000,"reason":"director"}
{"t":101300,"seq":17,"r":"RULE","art":"11.2.4.1","what":"resume_recalc","clock_ms":10000,"unshot":2,"per_arrow_ms":30000,"result_ms":60000,"reason":"flat_per_arrow"}
{"t":101400,"seq":18,"r":"RULE","art":"11.2.3.2","what":"handoff","from":1,"to":2,"banked_ms":0,"arrows_from":1,"reason":"director"}
{"t":101500,"seq":19,"r":"RULE","art":"11.2.3.2","what":"handoff","from":2,"to":1,"banked_ms":0,"arrows_from":1,"reason":"timeout"}
{"t":101600,"seq":20,"r":"HEALTH","heap":142000,"heap_min":128000,"heap_block":96000,"ap_clients":1}
"""

BAD_LOG = """\
{"t":10,"seq":1,"r":"BOOT","fw":"selftest","schema":3,"level":"MINIMAL"}
{"t":15,"seq":2,"r":"STATE","cause":"boot","schema":3,"phase":"IDLE","light":"OFF","mode":"IND_NONALT","disp":"CLOCK","rem_ms":60000,"per_ms":60000,"end":1,"set":0,"arrows":0,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":false,"fin":false,"snd":true,"bt":false,"bri":16}
{"t":16,"seq":3,"r":"RULE","art":"11.2.1.2","what":"period","arrows":3,"per_arrow_ms":30000,"period_ms":100000,"reason":"OTHER"}
{"t":20,"seq":4,"r":"STATE","cause":"start","schema":3,"phase":"SHOOTING","light":"RED","mode":"IND_NONALT","disp":"CLOCK","rem_ms":60000,"per_ms":60000,"end":1,"set":0,"arrows":0,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":true,"fin":false,"snd":true,"bt":false,"bri":16}
{"t":21,"seq":5,"r":"RENDER","content":"CLOCK","light":"RED","text":"00:15","crc":333,"lit":40}
{"t":30020,"seq":8,"r":"STATE","cause":"tick","schema":3,"phase":"WARNING","light":"YELLOW","mode":"IND_ALT","disp":"CLOCK","rem_ms":30000,"per_ms":60000,"end":1,"set":0,"arrows":1,"arrows_per_end":3,"shooter":1,"score":[0,0],"set_points":[0,0],"run":true,"fin":false,"snd":true,"bt":false,"bri":16}
{"t":40020,"seq":9,"r":"RULE","art":"11.2.4.1","what":"resume_recalc","clock_ms":9000,"unshot":2,"per_arrow_ms":30000,"result_ms":9000,"reason":"flat_per_arrow"}
{"t":40120,"seq":10,"r":"RULE","art":"11.2.2","what":"extend_time","before_ms":30000,"added_ms":15000,"after_ms":40000,"reason":"director"}
{"t":40220,"seq":11,"r":"RULE","art":"11.2.3.2","what":"handoff","from":1,"to":2,"banked_ms":0,"arrows_from":1,"reason":"director"}
{"t":40320,"seq":12,"r":"RULE","art":"11.2.3.2","what":"handoff","from":1,"to":2,"banked_ms":0,"arrows_from":1,"reason":"director"}
{"t":40420,"seq":13,"r":"RULE","art":"12.1.4.1","what":"set_points","end":1,"total_a":25,"total_b":27,"awarded_a":2,"awarded_b":0,"set_points_a":2,"set_points_b":0}
{"t":40520,"seq":14,"r":"RULE","art":"13.3","what":"forfeit_highest","side":0,"arrow_index":0,"lost_value":10,"before":27,"after":20,"reason":"judge decision"}
{"t":60020,"seq":15,"r":"SIGNAL","code":"STOP","beeps":1,"art":"11.3.1"}
{"t":60021,"seq":16,"r":"STATE","cause":"finish","schema":3,"phase":"FINISHED","light":"RED","mode":"IND_ALT","disp":"CLOCK","rem_ms":500,"per_ms":60000,"end":1,"set":0,"arrows":3,"arrows_per_end":3,"shooter":0,"score":[0,0],"set_points":[0,0],"run":false,"fin":true,"snd":true,"bt":false,"bri":16}
{"t":60022,"seq":17,"r":"ERR","code":"nvs","detail":"write failed"}
{"t":60023,"seq":18,"r":"HEALTH","heap":21000,"heap_min":14000,"heap_block":4000,"ap_clients":1}
"""


def selftest() -> int:
    good = run_checks(parse(GOOD_LOG.splitlines()))
    good_errors = [finding for finding in good if finding.severity == ERROR]
    if good_errors:
        print("selftest FAILED: a compliant session was rejected")
        for finding in good_errors:
            print("  " + finding.render())
        return 1

    bad = run_checks(parse(BAD_LOG.splitlines()))
    found = {finding.check for finding in bad if finding.severity == ERROR}
    expected = {
        "lights",
        "warning-signal",
        "signals",
        "state",
        "faults",
        "occupy",
        "period",
        "resume",
        "extension",
        "alternation",
        "set-points",
        "forfeit",
        "panel",
    }
    missing = expected - found
    if missing:
        print(f"selftest FAILED: these checks did not catch their planted fault: {sorted(missing)}")
        for finding in bad:
            print("  " + finding.render())
        return 1

    sequence_warnings = [finding for finding in bad if finding.check == "sequence"]
    if not sequence_warnings:
        print("selftest FAILED: the sequence gap in the bad log was not reported")
        return 1

    heap_warnings = [finding for finding in bad if finding.check == "heap"]
    if not heap_warnings:
        print("selftest FAILED: the starved heap in the bad log was not reported")
        return 1

    print(f"selftest passed: {len(CHECKS)} checks, compliant session clean, planted faults all caught")
    return 0


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("logfile", nargs="?", help="captured NDJSON trace; omit to read stdin")
    parser.add_argument("--strict", action="store_true", help="treat warnings as failures")
    parser.add_argument("--quiet", action="store_true", help="print only the summary")
    parser.add_argument("--json", action="store_true", help="emit the summary as JSON")
    parser.add_argument("--selftest", action="store_true", help="check the checker against known logs")
    arguments = parser.parse_args(argv)

    if arguments.selftest:
        return selftest()

    if arguments.logfile:
        with open(arguments.logfile, "r", encoding="utf-8", errors="replace") as handle:
            capture = parse(handle)
    else:
        capture = parse(sys.stdin)

    if not capture.records:
        print("no trace records found; is this a capture from a build with tracing enabled?")
        return 1

    findings = run_checks(capture)
    summary = summarise(capture, findings)

    if arguments.json:
        print(json.dumps({"summary": summary, "findings": [vars(f) for f in findings]}, indent=2))
    else:
        if not arguments.quiet:
            for finding in findings:
                if finding.severity == INFO and arguments.quiet:
                    continue
                print(finding.render())
        print(
            f"\n{summary['records']} records "
            f"({', '.join(f'{kind} {count}' for kind, count in sorted(summary['record_kinds'].items()))})"
        )
        if summary["skipped_lines"]:
            print(f"{summary['skipped_lines']} non-JSON line(s) skipped (framework output)")
        print(f"{summary['errors']} error(s), {summary['warnings']} warning(s), {summary['info']} note(s)")

    if summary["errors"]:
        return 1
    if arguments.strict and summary["warnings"]:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
