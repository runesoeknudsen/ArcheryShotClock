#pragma once

#include <Arduino.h>

#include "core/trace.h"

// UART sink for trace records.
//
// The point of the trace is to observe the timing engine, so it must not
// disturb it: a blocking Serial.write during a countdown would add jitter to
// the clock and to the RMT stream driving the WS2812B panel. Records are
// therefore copied into a ring buffer and drained from the main loop only as
// fast as the UART will take them without blocking.
//
// When the buffer is full, records are dropped and counted. The count is
// surfaced through takeDropped() so the caller can emit a WARN: a log that
// admits its gaps is usable, a log that quietly omits records is not.
//
// write() is not interrupt-safe. The emergency-stop button (Phase 2) records
// its own timestamp in the ISR and lets the main loop emit the record a tick
// later, so accuracy is preserved without tracing from interrupt context.
class SerialTraceSink : public Core::TraceSink {
public:
  static constexpr uint16_t CAPACITY = 4096;

  SerialTraceSink();

  void write(const char* line, uint16_t length) override;

  // Pushes buffered bytes to the UART without ever blocking. Call every loop.
  void drain();

  // Number of records dropped since the last call, then resets the counter.
  uint32_t takeDropped();

  uint16_t buffered() const;

private:
  uint16_t freeSpace() const;

  char buffer_[CAPACITY];
  uint16_t head_;
  uint16_t tail_;
  uint32_t dropped_;
};
