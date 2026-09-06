# Trace format

The firmware writes one JSON object per line to the UART (NDJSON). The stream is
the device's account of itself: every piece of state it holds, every command it
receives, every signal it emits, and every rulebook decision it makes.

It is meant to be read two ways — by eye in a serial monitor during a shoot, and
by `tools/logcheck.py` afterwards, which re-derives the logged decisions and
reports where the firmware got them wrong.

```text
pio device monitor -b 921600 > session.log
python3 tools/logcheck.py session.log
```

## Recording is off by default

Writing the trace costs time and memory on a board that is also running a Wi-Fi
access point, so it is off while an event is running and switched on in the web
UI under **Testing mode**. The setting persists.

| Level | What is written |
|---|---|
| `OFF` | `BOOT`, `WARN`, `ERR` only |
| `MINIMAL` | adds `STATE` on change, `RULE`, `SIGNAL`, `RENDER`, `CFG` — the evidence trail |
| `NORMAL` | adds `INPUT` and the 1 Hz heartbeat and `HEALTH` |
| `VERBOSE` | adds high-rate diagnostics; bench only |

Faults are never switchable off. A `WARN` or `ERR` that could be silenced would
be a fault nobody hears about, so those three record kinds are written at every
level — which also means a log captured during an event still shows anything
that went wrong.

## Envelope

Every record carries the same three fields:

| Field | Meaning |
|---|---|
| `t` | Milliseconds since boot, captured when the event happened, not when it was printed. Wraps after ~49.7 days. |
| `seq` | Monotonic counter, incremented by exactly one per emitted record. |
| `r` | Record kind. |

`seq` is what lets a reader tell a dropped record from a quiet period. If the
sink has to drop records under pressure it counts them and the firmware emits a
`WARN` with `code: "trace_dropped"`, so a gap in `seq` that nothing accounts for
is treated as lost evidence rather than shrugged off.

A record that would exceed `TRACE_LINE_MAX` is closed properly and marked
`"trunc":1`. A reader is never handed half an object.

## Record kinds

### `BOOT`
Emitted once at startup so a capture is self-describing.

```json
{"t":31,"seq":1,"r":"BOOT","fw":"esp-archery-timer 0.1.0","schema":1,"level":1}
```

`schema` is `Core::SCHEMA_VERSION`, currently **2**. Version 2 added the
alternating-shooting fields (shooting side, per-side arrows and per-side
clocks), the AB/CD detail number and the shoot-off marker. Readers refuse to
interpret a version they do not know rather than guessing.

### `STATE`
The complete `Core::StateSnapshot`. Emitted whenever any field changes, and
otherwise once a second as a heartbeat (`cause: "heartbeat"`), which is also
what makes a stalled main loop visible as a gap.

```json
{"t":10020,"seq":6,"r":"STATE","cause":"tick","schema":1,"phase":"SHOOTING",
 "light":"GREEN","mode":"IND_NONALT","disp":"CLOCK","rem_ms":120000,
 "per_ms":120000,"end":1,"set":0,"arrows":0,"arrows_per_end":3,"shooter":0,
 "score":[0,0],"set_points":[0,0],"run":true,"fin":false,"snd":true,"bri":16}
```

`cause` is a short reason for the change — `boot`, `start`, `pause`, `tick`,
`finish`, `heartbeat`, `change` — so a reader can see why the state moved
without diffing two lines.

`phase` is one of `IDLE`, `OCCUPY`, `SHOOTING`, `WARNING`, `FINISHED`,
`SCORING`, `SUSPENDED`, `EMERGENCY`. `light` is derived from the clock, never
tracked separately: Article 11.3.1 makes the digital clock authoritative if the
two ever disagree, so the pairing of phase and light is an invariant the log
checker enforces.

### `INPUT`
An operator action, from a button (`src: "btn"`) or the web UI (`src: "web"`).
`hold_ms` matters where a long press changes the meaning, otherwise `0`.

```json
{"t":20,"seq":3,"r":"INPUT","src":"web","ctl":"start","hold_ms":0}
```

### `SIGNAL`
A light or sound output, with the article that prescribes it. The counts in
Article 11.3 are load-bearing — officials are trained to recognise them — so
`beeps` is checked exactly.

```json
{"t":10020,"seq":7,"r":"SIGNAL","code":"START","beeps":1,"art":"11.3.1"}
```

| `code` | Sounds | Meaning |
|---|---|---|
| `OCCUPY_LINE` | 2 | Athletes occupy the shooting line |
| `START` | 1 | Shooting begins |
| `STOP` | 2 | Shooting time finished |
| `SCORING` | 3 | Scoring may begin |
| `EMERGENCY` | ≥5 | All shooting must cease (Art. 11.3.3) |
| `RESUME` | 1 | Resume after suspension |

The pre-rulebook `FINISH` alert is gone: the plain countdown was replaced by the
rulebook sequence, so `STOP` and `SCORING` now carry their proper counts. A code
outside this table is an error in any mode.

### `RULE`
A rulebook decision — and the most important record kind here.

```json
{"t":184320,"seq":812,"r":"RULE","art":"11.2.4.2","what":"resume_recalc",
 "clock_ms":38000,"unshot":3,"floor_ms":60000,"result_ms":60000,
 "reason":"clock<floor"}
```

It carries the **inputs** the decision was made from, not only its result. That
is what makes the log verifiable instead of merely informative: `logcheck.py`
recomputes the decision from the same inputs and disagrees when the firmware got
it wrong. A `RULE` record that only reported its outcome could not be checked by
anything except the code that produced it.

### `RENDER`
What the panel was told to show, emitted only when the frame changes.

```json
{"t":10020,"seq":9,"r":"RENDER","content":"CLOCK","light":"GREEN",
 "text":"01:30","crc":222,"lit":60}
```

`text` is what a person reading the panel would see, and `crc` identifies the
frame without printing 512 pixels. This is what separates "the clock is running
but the panel is dark" from "the panel was never drawn" — and the log checker
compares `text` against the clock in the preceding `STATE`, because Art. 11.3.1
makes the clock authoritative and a panel disagreeing with it means athletes are
reading something the firmware does not believe.

### `HEALTH`
Free heap, the lowest it has ever been, the largest free block and the number of
clients on the access point. Emitted with the heartbeat.

```json
{"t":101600,"seq":20,"r":"HEALTH","heap":142000,"heap_min":128000,
 "heap_block":96000,"ap_clients":1}
```

A fragmented heap and a bad radio environment look identical from a laptop —
both present as an access point that keeps dropping. These numbers are what
tell them apart, and the checker warns when free heap or the largest free block
falls far enough for the access point to start failing.

### `CFG`
A configuration change, old value to new. Numeric or textual.

```json
{"t":12,"seq":2,"r":"CFG","key":"duration_s","from":240,"to":120}
```

### `WARN` / `ERR`
Faults and anomalies. `WARN` with `code: "trace_dropped"` carries a `count` of
records the sink could not accept.

## Which decisions are recomputed

`tools/logcheck.py` does not take a `RULE` record's word for its result. It
re-derives these independently from the inputs in the same record:

| `what` | Article | Recomputed as |
|---|---|---|
| `period` | 11.2.1 | arrows × per-arrow, and the rate against the mode and event class |
| `resume_recalc` | 11.2.4.1 | unshot arrows × per-arrow, with no reference to the clock |
| `resume_recalc` | 11.2.4.2 | the clock if it beats 20 s × unshot arrows, otherwise that floor |
| `extend_time` | 11.2.2 | before + added |
| `handoff` | 11.2.3.2 / 11.1.4.3 | that the turn genuinely alternates |
| `set_points` | 12.1.4.1 / 12.1.4.2 | 2 to the higher total, 1 each when level, and the running sum |
| `forfeit_highest` | 13.3 / 13.6.2 | before − lost value |

Adding a decision without adding its row here means the log records it but
nothing checks it.

## What is deliberately not here

Free-form text. With `CORE_DEBUG_LEVEL` raised, the ESP-IDF and bluedroid stacks
write plain lines to the same UART; the reader counts and skips them. Firmware
code should not add to that — anything worth saying belongs in a record, so it
survives into the checked log.

## Adding a record

1. Add the emitter to `Core::Tracer` in `src/core/trace.h` and `trace.cpp`.
2. Cover its shape in `test/test_trace/test_main.cpp`.
3. Document it here.
4. If it encodes a rulebook decision, add the matching check to
   `tools/logcheck.py` and a row to `docs/rulebook-mapping.md`.

Changing the meaning of an existing field means bumping `Core::SCHEMA_VERSION`
and `SUPPORTED_SCHEMA` in the checker together.
