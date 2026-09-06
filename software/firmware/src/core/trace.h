#pragma once

#include <cstdint>

#include "snapshot.h"

// Structured trace records: the firmware's account of itself.
//
// Everything the device knows and every decision it makes is emitted here as
// one NDJSON object per line - readable over the serial monitor during a
// shoot, and parsable afterwards by tools/logcheck.py, which re-derives the
// rulebook decisions from the logged inputs and asserts they were correct.
//
// The core never touches Serial. It writes to a TraceSink, which is the UART
// on the device, a string buffer in the native tests, and (from Phase 4) a
// WebSocket feed for the web UI. One mechanism, three consumers, and the core
// stays free of Arduino.
//
// Record shape, common to every line:
//   {"t":<ms since boot>,"seq":<monotonic counter>,"r":"<kind>",...}
//
// "seq" increments by exactly one per emitted record, so a reader can tell a
// dropped record from a quiet period. See docs/trace-format.md.

namespace Core {

// Longest line the tracer will emit. Anything longer is truncated and flagged
// with "trunc":1 rather than being silently cut, so a reader is never misled
// by a half-written record.
constexpr uint16_t TRACE_LINE_MAX = 512;

enum class TraceLevel : uint8_t {
  // Running an event. Only boot, warnings and errors are written, so the UART
  // is quiet and the log buffer stays empty - but a fault is never invisible,
  // which is the one thing that must not be switchable off.
  Off = 0,
  // State changes, rulebook decisions, signals, renders and config changes:
  // the evidence trail. This is testing mode.
  Minimal = 1,
  // Adds operator input and the 1 Hz heartbeat.
  Normal = 2,
  // Adds high-rate diagnostics. For bench work, not for a live event.
  Verbose = 3
};

const char* name(TraceLevel level);
bool parseTraceLevel(const char* text, TraceLevel& level);

class TraceSink {
public:
  virtual ~TraceSink() = default;
  // Must not block. A sink that cannot accept the line drops it and counts it.
  virtual void write(const char* line, uint16_t length) = 0;
};

// A named integer carried by a RULE record: the inputs a rulebook decision was
// made from, so the decision can be recomputed independently from the log.
struct TraceField {
  const char* key;
  int32_t value;
};

class Tracer {
public:
  explicit Tracer(TraceSink& sink);

  void setLevel(TraceLevel level);
  TraceLevel level() const;

  // Number of records emitted so far; equals the "seq" of the last line.
  uint32_t sequence() const;
  // True if the last emitted line hit TRACE_LINE_MAX.
  bool lastLineTruncated() const;

  // True when anything beyond faults is being written. Callers use it to skip
  // work that only exists to be traced.
  bool isRecording() const { return level_ > TraceLevel::Off; }

  // Firmware identity and schema version. Emit once at boot so a captured log
  // is self-describing. Written at every level.
  void boot(uint32_t nowMs, const char* firmware);

  // Free heap and the smallest it has ever been. Fragmentation is what takes
  // the access point down, and it is invisible without a number.
  void health(uint32_t nowMs, uint32_t freeHeap, uint32_t minFreeHeap, uint32_t largestBlock, uint8_t apClients);

  // Full state. Emit whenever StateSnapshot::sameAs reports a change.
  void state(uint32_t nowMs, const StateSnapshot& snapshot, const char* cause);
  // The same record, but suppressed below TraceLevel::Normal.
  void heartbeat(uint32_t nowMs, const StateSnapshot& snapshot);

  // An operator action. source is "btn" or "web"; holdMs is the press duration
  // where it matters (long-press Stop), otherwise 0.
  void input(uint32_t nowMs, const char* source, const char* control, uint32_t holdMs);

  // A light or sound output, with the article that prescribes it.
  void signal(uint32_t nowMs, const char* code, uint8_t beeps, const char* article);

  // A rulebook decision: which article, what was decided, from which inputs,
  // and why. This is what makes the log verifiable rather than merely
  // informative - logcheck.py recomputes the result from the same fields.
  void rule(uint32_t nowMs, const char* article, const char* what, const TraceField* fields, uint8_t fieldCount,
            const char* reason = nullptr);

  // What the panel was told to show. Emitted on change only, and carries a
  // checksum rather than 512 pixels, so the log can answer "was the display
  // actually updated, and with what" without drowning in frame data.
  void render(uint32_t nowMs, const char* content, const char* light, const char* text, uint32_t checksum,
              uint16_t litPixels);

  // Configuration changes, old value to new.
  void config(uint32_t nowMs, const char* key, int32_t from, int32_t to);
  void configText(uint32_t nowMs, const char* key, const char* from, const char* to);

  void warn(uint32_t nowMs, const char* code, const char* detail);
  void error(uint32_t nowMs, const char* code, const char* detail);
  // Records the sink had to drop. Emitted by the owner of the sink once the
  // pressure has passed; a lossy log that admits its losses beats a log that
  // quietly omits them.
  void dropped(uint32_t nowMs, uint32_t count);

private:
  class Line {
  public:
    explicit Line(char* buffer);
    void key(const char* key);
    void number(const char* key, int32_t value);
    void unsigned32(const char* key, uint32_t value);
    void text(const char* key, const char* value);
    void boolean(const char* key, bool value);
    void raw(const char* key, const char* rawValue);
    uint16_t finish();
    bool overflowed() const;

  private:
    void put(char character);
    void append(const char* text);
    void appendEscaped(const char* text);
    void separator();

    char* buffer_;
    uint16_t length_;
    bool overflow_;
    bool first_;
  };

  Line begin(uint32_t nowMs, const char* kind);
  void emit(Line& line);

  TraceSink& sink_;
  TraceLevel level_;
  uint32_t sequence_;
  bool truncated_;
  char buffer_[TRACE_LINE_MAX];
};

}  // namespace Core
