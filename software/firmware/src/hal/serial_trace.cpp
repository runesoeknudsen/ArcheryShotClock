#include "hal/serial_trace.h"

SerialTraceSink::SerialTraceSink() : head_(0), tail_(0), dropped_(0) {}

uint16_t SerialTraceSink::buffered() const {
  return head_ >= tail_ ? static_cast<uint16_t>(head_ - tail_) : static_cast<uint16_t>(CAPACITY - tail_ + head_);
}

uint16_t SerialTraceSink::freeSpace() const {
  // One byte is kept unused so head == tail always means empty.
  return static_cast<uint16_t>(CAPACITY - 1 - buffered());
}

void SerialTraceSink::write(const char* line, uint16_t length) {
  if (length == 0) return;
  if (length > freeSpace()) {
    // Drop whole records, never half of one: a truncated line would look like
    // corruption to a reader rather than a known, counted loss.
    dropped_++;
    return;
  }

  for (uint16_t index = 0; index < length; index++) {
    buffer_[head_] = line[index];
    head_ = static_cast<uint16_t>((head_ + 1) % CAPACITY);
  }
}

void SerialTraceSink::drain() {
  while (head_ != tail_) {
    const int room = Serial.availableForWrite();
    if (room <= 0) return;

    const uint16_t contiguous = head_ > tail_ ? static_cast<uint16_t>(head_ - tail_) : static_cast<uint16_t>(CAPACITY - tail_);
    const uint16_t chunk = contiguous < static_cast<uint16_t>(room) ? contiguous : static_cast<uint16_t>(room);
    const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(buffer_ + tail_), chunk);
    if (written == 0) return;

    tail_ = static_cast<uint16_t>((tail_ + written) % CAPACITY);
  }
}

uint32_t SerialTraceSink::takeDropped() {
  const uint32_t count = dropped_;
  dropped_ = 0;
  return count;
}
