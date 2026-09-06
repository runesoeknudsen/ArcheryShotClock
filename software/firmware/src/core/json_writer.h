#pragma once

#include <cstdint>

// Writes JSON into a caller-provided buffer. No heap, no String, no growth.
//
// This exists because building responses with String concatenation on an
// ESP32 that is also running a Wi-Fi access point is not merely slow: each
// += reallocates, and doing that a few hundred times a second fragments the
// heap until the access point itself starts failing. A fixed buffer written
// once cannot do that.
//
// Overflow is reported, never silent. A caller that ignores it gets a
// well-formed but incomplete document rather than a truncated one.

namespace Core {

class JsonWriter {
public:
  static constexpr uint8_t MAX_DEPTH = 4;

  // reserve keeps that many bytes free at the end of the buffer so finish()
  // can always close every open container. One byte per possible depth, plus
  // the terminator.
  JsonWriter(char* buffer, uint16_t capacity, uint16_t reserve = MAX_DEPTH + 1);

  void beginObject();
  void endObject();
  void beginObject(const char* key);
  void beginArray(const char* key);
  void endArray();

  void number(const char* key, int32_t value);
  void unsigned32(const char* key, uint32_t value);
  void text(const char* key, const char* value);
  void boolean(const char* key, bool value);
  void raw(const char* key, const char* rawValue);
  // Two numbers as an array, the shape used for anything held per side.
  void pair(const char* key, uint32_t first, uint32_t second);

  void arrayNumber(uint32_t value);
  void arrayText(const char* value);
  void arrayBoolean(bool value);

  bool overflowed() const { return overflow_; }
  uint16_t length() const { return length_; }
  // Closes any open containers and null-terminates. Returns the length.
  uint16_t finish();

private:
  void put(char character);
  void append(const char* text);
  void appendEscaped(const char* text);
  void separator();
  // Whole tokens are written or refused. A half-written key would leave an
  // unterminated string, which is worse to read than a value that is missing.
  bool fits(uint16_t bytes);
  uint16_t escapedLength(const char* text) const;
  uint16_t keyOverhead(const char* key) const;

  char* buffer_;
  uint16_t capacity_;
  uint16_t reserve_;
  uint16_t length_;
  bool overflow_;
  bool first_[MAX_DEPTH];
  char closers_[MAX_DEPTH];
  uint8_t depth_;
};

}  // namespace Core
