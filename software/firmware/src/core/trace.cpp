#include "trace.h"

#include <cstdio>

namespace Core {
namespace {
// Tail room kept free so finish() can always close the object, and add the
// truncation marker, even when the body filled the buffer.
constexpr uint16_t RESERVE = 16;

void formatPair(char* target, uint8_t size, uint32_t first, uint32_t second) {
  std::snprintf(target, size, "[%lu,%lu]", static_cast<unsigned long>(first), static_cast<unsigned long>(second));
}
}  // namespace

Tracer::Line::Line(char* buffer) : buffer_(buffer), length_(0), overflow_(false), first_(true) { put('{'); }

void Tracer::Line::put(char character) {
  if (length_ + RESERVE >= TRACE_LINE_MAX) {
    overflow_ = true;
    return;
  }
  buffer_[length_++] = character;
}

void Tracer::Line::append(const char* text) {
  for (const char* cursor = text; cursor && *cursor; cursor++) put(*cursor);
}

void Tracer::Line::appendEscaped(const char* text) {
  for (const char* cursor = text; cursor && *cursor; cursor++) {
    const char character = *cursor;
    if (character == '"' || character == '\\') {
      put('\\');
      put(character);
    } else if (character == '\n') {
      append("\\n");
    } else if (character == '\r') {
      append("\\r");
    } else if (character == '\t') {
      append("\\t");
    } else if (static_cast<unsigned char>(character) < 0x20) {
      char escape[7];
      std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(character)));
      append(escape);
    } else {
      put(character);
    }
  }
}

void Tracer::Line::separator() {
  if (!first_) put(',');
  first_ = false;
}

void Tracer::Line::key(const char* key) {
  separator();
  put('"');
  append(key);
  put('"');
  put(':');
}

void Tracer::Line::number(const char* keyName, int32_t value) {
  key(keyName);
  char digits[12];
  std::snprintf(digits, sizeof(digits), "%ld", static_cast<long>(value));
  append(digits);
}

void Tracer::Line::unsigned32(const char* keyName, uint32_t value) {
  key(keyName);
  char digits[12];
  std::snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(value));
  append(digits);
}

void Tracer::Line::text(const char* keyName, const char* value) {
  key(keyName);
  put('"');
  appendEscaped(value ? value : "");
  put('"');
}

void Tracer::Line::boolean(const char* keyName, bool value) {
  key(keyName);
  append(value ? "true" : "false");
}

void Tracer::Line::raw(const char* keyName, const char* rawValue) {
  key(keyName);
  append(rawValue);
}

bool Tracer::Line::overflowed() const { return overflow_; }

uint16_t Tracer::Line::finish() {
  if (overflow_) {
    const char* marker = ",\"trunc\":1";
    for (const char* cursor = marker; *cursor; cursor++) buffer_[length_++] = *cursor;
  }
  buffer_[length_++] = '}';
  buffer_[length_++] = '\n';
  buffer_[length_] = '\0';
  return length_;
}

const char* name(TraceLevel level) {
  switch (level) {
    case TraceLevel::Off: return "OFF";
    case TraceLevel::Minimal: return "MINIMAL";
    case TraceLevel::Normal: return "NORMAL";
    case TraceLevel::Verbose: return "VERBOSE";
  }
  return "UNKNOWN";
}

bool parseTraceLevel(const char* text, TraceLevel& level) {
  if (text == nullptr) return false;
  for (uint8_t candidate = 0; candidate <= static_cast<uint8_t>(TraceLevel::Verbose); candidate++) {
    const TraceLevel option = static_cast<TraceLevel>(candidate);
    const char* optionName = name(option);
    const char* left = text;
    const char* right = optionName;
    while (*left && *right && *left == *right) {
      left++;
      right++;
    }
    if (*left == '\0' && *right == '\0') {
      level = option;
      return true;
    }
  }
  return false;
}

Tracer::Tracer(TraceSink& sink) : sink_(sink), level_(TraceLevel::Off), sequence_(0), truncated_(false) {
  buffer_[0] = '\0';
}

void Tracer::setLevel(TraceLevel level) { level_ = level; }
TraceLevel Tracer::level() const { return level_; }
uint32_t Tracer::sequence() const { return sequence_; }
bool Tracer::lastLineTruncated() const { return truncated_; }

Tracer::Line Tracer::begin(uint32_t nowMs, const char* kind) {
  Line line(buffer_);
  line.unsigned32("t", nowMs);
  line.unsigned32("seq", sequence_ + 1);
  line.text("r", kind);
  return line;
}

void Tracer::emit(Line& line) {
  sequence_++;
  truncated_ = line.overflowed();
  const uint16_t length = line.finish();
  sink_.write(buffer_, length);
}

void Tracer::boot(uint32_t nowMs, const char* firmware) {
  Line line = begin(nowMs, "BOOT");
  line.text("fw", firmware);
  line.unsigned32("schema", SCHEMA_VERSION);
  line.text("level", name(level_));
  emit(line);
}

void Tracer::state(uint32_t nowMs, const StateSnapshot& snapshot, const char* cause) {
  if (level_ == TraceLevel::Off) return;
  Line line = begin(nowMs, "STATE");
  line.text("cause", cause);
  line.unsigned32("schema", snapshot.schemaVersion);
  line.text("phase", name(snapshot.phase));
  line.text("light", name(snapshot.light));
  line.text("mode", name(snapshot.mode));
  line.text("disp", name(snapshot.display));
  line.unsigned32("rem_ms", snapshot.remainingMs);
  line.unsigned32("per_ms", snapshot.periodMs);
  line.unsigned32("end", snapshot.endNumber);
  line.unsigned32("set", snapshot.setNumber);
  line.unsigned32("arrows", snapshot.arrowsShot);
  line.unsigned32("arrows_per_end", snapshot.arrowsPerEnd);
  line.unsigned32("shooter", snapshot.shooter);

  char pair[24];
  formatPair(pair, sizeof(pair), snapshot.score[0], snapshot.score[1]);
  line.raw("score", pair);
  formatPair(pair, sizeof(pair), snapshot.setPoints[0], snapshot.setPoints[1]);
  line.raw("set_points", pair);

  line.boolean("run", snapshot.running);
  line.boolean("fin", snapshot.finished);
  line.boolean("snd", snapshot.soundEnabled);
  line.unsigned32("bri", snapshot.brightness);
  emit(line);
}

void Tracer::heartbeat(uint32_t nowMs, const StateSnapshot& snapshot) {
  if (level_ < TraceLevel::Normal) return;
  state(nowMs, snapshot, "heartbeat");
}

void Tracer::input(uint32_t nowMs, const char* source, const char* control, uint32_t holdMs) {
  if (level_ < TraceLevel::Normal) return;
  Line line = begin(nowMs, "INPUT");
  line.text("src", source);
  line.text("ctl", control);
  line.unsigned32("hold_ms", holdMs);
  emit(line);
}

void Tracer::signal(uint32_t nowMs, const char* code, uint8_t beeps, const char* article) {
  if (level_ == TraceLevel::Off) return;
  Line line = begin(nowMs, "SIGNAL");
  line.text("code", code);
  line.unsigned32("beeps", beeps);
  line.text("art", article);
  emit(line);
}

void Tracer::rule(uint32_t nowMs, const char* article, const char* what, const TraceField* fields, uint8_t fieldCount,
                  const char* reason) {
  if (level_ == TraceLevel::Off) return;
  Line line = begin(nowMs, "RULE");
  line.text("art", article);
  line.text("what", what);
  for (uint8_t index = 0; index < fieldCount && fields; index++) {
    line.number(fields[index].key, fields[index].value);
  }
  if (reason) line.text("reason", reason);
  emit(line);
}

void Tracer::render(uint32_t nowMs, const char* content, const char* light, const char* text, uint32_t checksum,
                    uint16_t litPixels) {
  if (level_ == TraceLevel::Off) return;
  Line line = begin(nowMs, "RENDER");
  line.text("content", content);
  line.text("light", light);
  line.text("text", text);
  line.unsigned32("crc", checksum);
  line.unsigned32("lit", litPixels);
  emit(line);
}

void Tracer::config(uint32_t nowMs, const char* key, int32_t from, int32_t to) {
  if (level_ == TraceLevel::Off) return;
  Line line = begin(nowMs, "CFG");
  line.text("key", key);
  line.number("from", from);
  line.number("to", to);
  emit(line);
}

void Tracer::configText(uint32_t nowMs, const char* key, const char* from, const char* to) {
  if (level_ == TraceLevel::Off) return;
  Line line = begin(nowMs, "CFG");
  line.text("key", key);
  line.text("from", from);
  line.text("to", to);
  emit(line);
}

void Tracer::health(uint32_t nowMs, uint32_t freeHeap, uint32_t minFreeHeap, uint32_t largestBlock,
                    uint8_t apClients) {
  if (level_ < TraceLevel::Normal) return;
  Line line = begin(nowMs, "HEALTH");
  line.unsigned32("heap", freeHeap);
  line.unsigned32("heap_min", minFreeHeap);
  line.unsigned32("heap_block", largestBlock);
  line.unsigned32("ap_clients", apClients);
  emit(line);
}

void Tracer::warn(uint32_t nowMs, const char* code, const char* detail) {
  Line line = begin(nowMs, "WARN");
  line.text("code", code);
  line.text("detail", detail);
  emit(line);
}

void Tracer::error(uint32_t nowMs, const char* code, const char* detail) {
  Line line = begin(nowMs, "ERR");
  line.text("code", code);
  line.text("detail", detail);
  emit(line);
}

void Tracer::dropped(uint32_t nowMs, uint32_t count) {
  Line line = begin(nowMs, "WARN");
  line.text("code", "trace_dropped");
  line.unsigned32("count", count);
  emit(line);
}

}  // namespace Core
