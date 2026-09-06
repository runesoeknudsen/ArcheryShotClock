#pragma once

#include <cstdint>

// Debounce and long-press detection, kept away from Arduino so the timing can
// be tested on the development machine rather than by pressing buttons.
//
// A bounce that produced a second Start press mid-end, or a missed press that
// left athletes on the line with no clock, would both be visible to everyone on
// the field. This is small enough to get exactly right and worth doing so.

namespace Core {

enum class ButtonEvent : uint8_t { None, Press, LongPress, Release };

class ButtonDebouncer {
public:
  // Defaults: 25 ms is comfortably longer than a tactile switch bounces;
  // 800 ms is long enough that a long press cannot be given by accident and
  // short enough not to feel broken.
  explicit ButtonDebouncer(uint16_t debounceMs = 25, uint16_t longPressMs = 800);

  // rawPressed is the electrical state with the pull-up already inverted.
  ButtonEvent update(uint32_t now, bool rawPressed);

  bool isPressed() const { return stable_; }
  uint32_t heldMs(uint32_t now) const { return stable_ ? now - stableSince_ : 0; }

private:
  uint16_t debounceMs_;
  uint16_t longPressMs_;
  bool stable_;
  bool candidate_;
  bool longPressSent_;
  uint32_t candidateSince_;
  uint32_t stableSince_;
};

}  // namespace Core
