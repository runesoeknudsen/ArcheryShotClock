#pragma once

#include <cstdint>

#include "core/trace.h"

// Keeps the most recent trace records in RAM so the web UI can show the same
// log the UART carries. A director with a tablet and no serial cable should
// still be able to see why the clock did what it did.
class LogBufferSink : public Core::TraceSink {
public:
  static constexpr uint8_t CAPACITY = 32;
  static constexpr uint16_t LINE_BYTES = 240;

  LogBufferSink();

  void write(const char* line, uint16_t length) override;

  uint8_t count() const { return count_; }
  // Oldest first; index must be below count().
  const char* line(uint8_t index) const;
  // The record's own seq, so the console can ask for only what it is missing
  // instead of being sent the whole buffer every time it polls.
  uint32_t sequence(uint8_t index) const;

private:
  char lines_[CAPACITY][LINE_BYTES];
  uint32_t sequences_[CAPACITY];
  uint8_t next_;
  uint8_t count_;
};

// Fans one trace stream out to two sinks. Used so the UART and the web UI see
// exactly the same records rather than two separately-generated logs.
class TeeSink : public Core::TraceSink {
public:
  TeeSink(Core::TraceSink& first, Core::TraceSink& second);
  void write(const char* line, uint16_t length) override;

private:
  Core::TraceSink& first_;
  Core::TraceSink& second_;
};
