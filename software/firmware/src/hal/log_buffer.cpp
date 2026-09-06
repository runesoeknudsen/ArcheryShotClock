#include "hal/log_buffer.h"

namespace {
// Pulls "seq":N out of a record without parsing the whole object.
uint32_t sequenceOf(const char* line, uint16_t length) {
  const char* marker = "\"seq\":";
  for (uint16_t index = 0; index + 6 < length; index++) {
    bool matched = true;
    for (uint8_t offset = 0; offset < 6; offset++) {
      if (line[index + offset] != marker[offset]) {
        matched = false;
        break;
      }
    }
    if (!matched) continue;

    uint32_t value = 0;
    uint16_t cursor = index + 6;
    while (cursor < length && line[cursor] >= '0' && line[cursor] <= '9') {
      value = value * 10 + static_cast<uint32_t>(line[cursor] - '0');
      cursor++;
    }
    return value;
  }
  return 0;
}
}  // namespace

LogBufferSink::LogBufferSink() : next_(0), count_(0) {
  for (uint8_t index = 0; index < CAPACITY; index++) {
    lines_[index][0] = '\0';
    sequences_[index] = 0;
  }
}

void LogBufferSink::write(const char* line, uint16_t length) {
  if (length == 0) return;
  // Drop the trailing newline; the web UI adds its own line breaks and a raw
  // newline inside the JSON response would have to be escaped anyway.
  uint16_t copyLength = length;
  while (copyLength > 0 && (line[copyLength - 1] == '\n' || line[copyLength - 1] == '\r')) copyLength--;
  if (copyLength > LINE_BYTES - 1) copyLength = LINE_BYTES - 1;

  for (uint16_t index = 0; index < copyLength; index++) lines_[next_][index] = line[index];
  lines_[next_][copyLength] = '\0';
  sequences_[next_] = sequenceOf(line, copyLength);

  next_ = static_cast<uint8_t>((next_ + 1) % CAPACITY);
  if (count_ < CAPACITY) count_++;
}

const char* LogBufferSink::line(uint8_t index) const {
  if (index >= count_) return "";
  const uint8_t oldest = count_ < CAPACITY ? 0 : next_;
  return lines_[(oldest + index) % CAPACITY];
}

uint32_t LogBufferSink::sequence(uint8_t index) const {
  if (index >= count_) return 0;
  const uint8_t oldest = count_ < CAPACITY ? 0 : next_;
  return sequences_[(oldest + index) % CAPACITY];
}

TeeSink::TeeSink(Core::TraceSink& first, Core::TraceSink& second) : first_(first), second_(second) {}

void TeeSink::write(const char* line, uint16_t length) {
  first_.write(line, length);
  second_.write(line, length);
}
