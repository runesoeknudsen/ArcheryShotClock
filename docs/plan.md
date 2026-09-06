# Plan: World Archery compliant shot clock

The shooting sequence for each timing program is in
[`timing-programs.md`](timing-programs.md).

**Status:** Phases 0 to 6 implemented on a single ESP32. What remains is field
work, not code: see §6 Phase 6 and the hardware checks in §7.

Target: bring the existing ESP32 + 32x16 WS2812B timer up to World Archery Book 3
(Ch. 11–14) behaviour and the functional scope of `ArcAndAlgorithm/ArcheryShotClock`,
**on a single ESP32** (controller + display + buzzer in one box).

Decisions taken as given:

- **Scope:** full timing engine *and* match logic (scores, set play, cumulative).
- **Controls:** physical buttons *and* web UI; emergency stop is a real button.
- **Display:** 32x16 shows the clock by default; what it shows is selectable from the web UI.
- **Web UI:** everything the system knows is visible there.
- Multi-device (ESP-NOW) is out of scope now, but the design must not block it.

---

## 1. Gap analysis

| Area | Today | Needed |
|---|---|---|
| Clock | Single countdown, start/pause/reset | Phase state machine: occupy-line → shooting → warning → finished → scoring |
| Duration | One configurable value | Derived from mode: arrows × per-arrow time (20/30/40 s) |
| Lights | None | RED / GREEN / YELLOW rendered as panel colour (Art. 11.3.1) |
| Sound | 1 pulse start, 3 pulses finish | Exact WA patterns: 2 / 1 / 2 / 3 / ≥5 / 1 signals |
| Modes | One | Individual non-alt, individual alternating, team, mixed team, team alternating, AB/CD rotation, shoot-off, practice |
| Pause | Plain unpause | Art. 11.2.4 recalculation (differs individual vs team) + 10 s signal replayed on resume |
| Emergency | None | Dedicated button, interrupt-driven, independent of Wi-Fi |
| Match logic | None | End/set counters, set play (6 pts ind. / 5 pts team), cumulative (5 ends ind. / 4 team), arrow entry, Ch. 13 tools |
| Input | Web only | Start / Stop / Pause / Next End / Reset End / E-stop buttons |
| Log | None | Timestamped event log for disputes |

The good news: the repo already separates deterministic logic from Arduino and has a
native test target (`pio test -e native`) plus Playwright web tests. That is exactly the
right substrate — the plan leans on it hard.

---

## 2. Target source layout

```
src/
  config.h              # pins, geometry, defaults (extended)
  main.cpp              # wiring only: poll inputs -> core -> render/sound/web
  core/                 # ARDUINO-FREE, 100% native-testable
    rules.h             #   WA constants + traceability comments (article -> constant)
    signals.h           #   SignalCode enum + beep patterns
    shot_clock.h/.cpp   #   phase state machine (the clock itself)
    session.h/.cpp      #   mode config, end/set sequencing, alternating handoff, rotation
    match_logic.h/.cpp  #   arrows, scores, set points, cumulative, Ch. 13 tools
    snapshot.h          #   StateSnapshot struct (+ schema_version) = the one state object
    event_log.h/.cpp    #   fixed-size ring buffer of timestamped events
    trace.h/.cpp        #   structured trace records + pluggable sink (see §5)
  hal/
    display.h/.cpp      #   renders a StateSnapshot; content selectable
    sound.h/.cpp        #   plays a SignalCode as a pattern (buzzer + MAX98357A)
    buttons.h/.cpp      #   debounced GPIO; E-stop on ISR
    settings.h/.cpp     #   NVS
    serial_trace.h/.cpp #   UART sink for trace records (non-blocking, buffered)
  net/
    web_ui.h/.cpp       #   HTTP config + WebSocket state push
web/index.html          # rebuilt UI, still generated into src/web_page.h at build
tools/
  logcheck.py           # parses a captured session log, asserts rulebook conformance
```

`StateSnapshot` is the key idea: the core produces one serialisable struct per tick;
display, sound, web UI and (later) ESP-NOW are all just consumers of it. Give it a
`schema_version` field from day one — that is what keeps the multi-device door open for
essentially zero cost.

---

## 3. Rules to encode (`core/rules.h`)

Each constant carries its article number in a comment so compliance is auditable.

- **Per-arrow time:** 40 s, reducible to 30 s (Art. 11.2.1.2); 30 s at WRE/international
  (11.2.1.1); **20 s** for alternating individual, team, and mixed team (11.1.4.1–.3).
- **End size:** 3 or 6 arrows individual (10.1); 6 team, 4 mixed team.
- **Occupy-line phase:** 10 s, RED + 2 sound signals; then GREEN + 1 signal.
- **Yellow warning:** 30 s remaining — *suppressed* in alternating finals (periods are
  only 20 s, so the warning is meaningless there).
- **Finish:** RED + 2 signals at 0, or on Stop. Then 3 signals = scoring may begin.
- **Emergency:** ≥5 signals, halts everything.
- **Resume after suspension:** 1 signal, plus the 10 s occupy-line signal replayed.
- **Set play:** first to 6 set points individual (12.1.4.1), 5 team (12.1.4.2).
- **Cumulative:** 5 ends individual (12.1.4.3), 4 ends team (12.1.4.4).
- **Suspension recalculation (11.2.4):**
  - Individual → unshot arrows each get their full allocation back.
  - Team/mixed → if remaining clock > (20 s × unshot arrows), keep the clock; else reset
    to 20 s × unshot arrows.

That second rule is why an **arrow counter is mandatory even with scoring off** — a "+1
arrow" control the director taps, independent of score entry.

---

## 4. Hardware additions

| Control | Suggested GPIO | Note |
|---|---|---|
| Start / Handoff | 32 | Doubles as the alternating-shooting handoff |
| Stop | 33 | Long-press = "match conceded", to avoid accidents |
| Pause / Resume | 25 | |
| Next End | 26 | |
| Reset Current End | 14 | |
| **Emergency Stop** | 4 | ISR-driven; use a **normally-closed** switch so a broken wire fails safe |

Already used: GPIO 13 (LED data), GPIO 27 (buzzer), GPIO 18/19/23/16 (MAX98357A I2S).
Avoid 34–39 (no internal pull-ups) and the strapping pins (0, 2, 12, 15). All buttons
use `INPUT_PULLUP` and are debounced in software (~25 ms); the E-stop additionally
latches in the ISR so a bounce can never lose the press.

**Risk to watch:** I2S to a MAX98357A + Wi-Fi AP + WS2812B on one ESP32-WROOM still
shares the radio with RMT LED timing, but it no longer holds the Bluetooth Classic
controller. Keep the I2S clock on core 1 and budget a field test for the amp, the
buzzer and the panels together before relying on it at an event. Flash stays enlarged
in `partitions.csv` for the web UI and match logic, so keep an eye on the build size.

---

## 5. Observability: everything on the UART

The whole internal state and every decision goes out over serial, in a form a human can
read during a field test *and* a script can verify afterwards. This is the primary
evidence that the rules are actually being followed.

### 5.1 How it works

The core never calls `Serial.print`. It emits **trace records** to an injected sink:

- on device, the sink is `hal/serial_trace` (UART at 921600 baud);
- in native tests, the sink is a string buffer the test asserts against;
- the web UI can subscribe to the same stream over WebSocket, so the log is visible
  without a USB cable.

One mechanism, three consumers — and the core stays Arduino-free and native-testable.

### 5.2 What gets emitted

| Record | When | Contains |
|---|---|---|
| `STATE` | On any state change, plus a 1 Hz heartbeat | Full `StateSnapshot`: phase, light, remaining ms, mode, end/set, arrows shot, current shooter, scores, set points, display content, connection state |
| `INPUT` | Every button press and web command | Source (`btn`/`web`), control, press duration |
| `SIGNAL` | Every sound/light output | Signal code, beep count, pattern timing |
| `RULE` | Every decision the rulebook drives | The article, the inputs to the decision, the result |
| `CFG` | Session setup / any setting change | Old value → new value |
| `WARN` / `ERR` | Anomalies | Dropped trace records, timing overrun, NVS failure |

`RULE` is the one that makes the log *verifiable* rather than merely informative. Any
computed duration, any recalculation, any auto-declared winner states which article
produced it and what it was given:

```
{"t":184320,"r":"RULE","art":"11.2.4.2","what":"resume_recalc","clock_ms":38000,"unshot":3,"floor_ms":60000,"result_ms":60000,"reason":"clock<floor"}
{"t":184321,"r":"STATE","phase":"OCCUPY","light":"RED","rem_ms":10000,"mode":"TEAM_SIMUL","end":3,"arrows":3,"score":[87,84],"disp":"CLOCK"}
{"t":194322,"r":"SIGNAL","code":"START","beeps":1,"art":"11.3.1"}
```

NDJSON: one self-contained JSON object per line. Greppable by eye, trivially parsed by
`tools/logcheck.py`, and a truncated line can never corrupt the ones around it. A
`--pretty` build flag renders the same records as aligned columns if you prefer reading
raw serial during a shoot.

Verbosity is a runtime setting (web UI) with a compile-time floor: `STATE`-on-change +
`RULE` + `SIGNAL` always on, heartbeat and `INPUT` toggleable, per-tick clock dumps
behind a debug flag.

### 5.3 `tools/logcheck.py` — rulebook conformance from a captured log

Point it at a captured session (`pio device monitor > session.log`) and it asserts the
invariants that a unit test can't easily reach because they span a whole round:

- GREEN is never entered without a preceding 10 s RED occupy phase, with 2 signals
  then 1 (Art. 11.3.1);
- YELLOW appears at exactly 30 s remaining, and **never** in alternating modes;
- signal counts are exactly 2 / 1 / 2 / 3 / ≥5 / 1 for their respective events;
- period length always equals arrows × per-arrow time for the active mode;
- every resume recalculation matches the article it claims (recompute it independently
  from the logged inputs and compare);
- alternating handoffs always alternate, and a timeout auto-advances;
- set-play/cumulative totals and the declared winner are recomputable from the logged
  arrow values;
- no `ERR`, and no dropped trace records.

The same checker runs over native-test output, so one set of assertions covers both
simulated and real sessions. A field test then produces a log you can hand to a judge as
evidence of what the system did and when.

### 5.4 Don't let logging break the thing it's measuring

Serial writes must never add jitter to the clock or the WS2812B RMT stream:

- the UART sink writes into a ring buffer and drains from the main loop — never blocks,
  never writes from an ISR (the E-stop ISR sets a flag; the trace is emitted a tick later
  with the ISR's own timestamp so accuracy is preserved);
- on buffer overflow, drop records and emit a single `WARN` with a dropped count — a
  silently incomplete log would be worse than an admittedly lossy one;
- timestamps come from a monotonic microsecond source captured at the moment of the
  event, not at the moment of printing;
- budget the worst case: heartbeat + a busy alternating end is a few hundred bytes per
  second, comfortably inside 921600 baud, but verify with the debug flag on.

---

## 6. Phasing

Each phase ends green on `pio test -e native` and `npm test`, and is independently
useful on the field.

### Phase 0 — Refactor + trace infrastructure, no behaviour change ✅
Split `timer.*` into `core/` and `hal/`, introduce `StateSnapshot`, **and stand up the
trace sink and NDJSON serial output** (§5) around the existing countdown. Keep behaviour
identical; existing tests pass untouched. Building the log first means every later phase
is observable from its first commit rather than retrofitted.

### Phase 1 — WA signal engine, one mode ✅
Phase state machine + colour rendering + correct beep patterns, for **individual
non-alternating qualification** only (Art. 11.2.1/11.3.1), each decision emitting its
`RULE` record. First version of `tools/logcheck.py` covering the §5.3 signal-sequence
invariants. At the end of this phase the box is usable for a normal qualification round
and can prove it behaved correctly.

### Phase 2 — Physical buttons ✅
Debounced inputs, E-stop on an interrupt path that does not touch Wi-Fi or the web
server. Verify E-stop still fires with the AP disabled and with a client mid-request —
and that its logged timestamp is the ISR's, not the print's.

### Phase 3 — Remaining timing modes ✅
Alternating individual (20 s, Start-as-handoff, auto-advance on timeout), team and mixed
team simultaneous, team alternating, AB/CD rotation flag, shoot-off, practice. Then
suspension/resume recalculation, which needs the arrow counter from §3.

### Phase 4 — Web UI rebuild ✅
Session setup (event type, round type, division, event class, arrows per end), live
mirror of clock/lights/end/arrow/scores, **display-content selector** (clock, clock +
end, arrow count, score, set points, shooter A/B, blank — default clock), manual time
extension (11.2.2), remote equivalents of every button, and a **live log pane** subscribed
to the same trace stream as the UART. WebSocket for live state, HTTP/JSON for config.
Stay on vanilla JS — flash is limited.

### Phase 5 — Match logic ✅
Arrow entry (0/M, 1–10, X), set-play and cumulative scoring with auto-declared winner,
unshot-arrow-is-a-miss handling (12.2.2.3), and the Chapter 13 director tools: forfeit
highest arrow, warning tracker, DSQ marker. All judge-triggered — the system records
decisions, it never adjudicates.

### Phase 6 — Hardening ✅ (bench) / outstanding (field)
Full-round field test **captured to a log and run through `logcheck.py` clean**, settings
persistence for every new option, brightness/readability check in direct sun, a written
rulebook-mapping doc (article → code location → trace record), and a build-size check.

---

## 7. Testing

- **Native (`pio test -e native`)** carries the weight: every timing mode driven by an
  injected clock, every signal sequence, both suspension recalculations, set-play and
  cumulative termination, and the display frame for each content mode. Aim for a test per
  rulebook article, named after the article.
- **`tools/logcheck.py`** (§5.3) runs over both native-test trace output and real captured
  sessions, so the same conformance assertions apply to simulated and field runs.
- **Playwright** covers the web UI against a mocked API, as it does now.
- **On-hardware checks** that no test can replace: E-stop with Wi-Fi down, MAX98357A +
  Wi-Fi + LED coexistence, sunlight readability, NVS survival across reboot, and clock
  drift over a full 2-hour session — all evidenced by a captured log.

---

## 8. Deliberately deferred

Multi-device ESP-NOW, dedicated buzzer nodes, PA line-out, scoring-display pairing,
companion apps. All of these become transport-and-packaging work once `StateSnapshot`
exists and is versioned — which is the one concession Phase 0 makes to them.
