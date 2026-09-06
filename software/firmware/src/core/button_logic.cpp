#include "button_logic.h"

namespace Core {

ButtonDebouncer::ButtonDebouncer(uint16_t debounceMs, uint16_t longPressMs)
    : debounceMs_(debounceMs),
      longPressMs_(longPressMs),
      stable_(false),
      candidate_(false),
      longPressSent_(false),
      candidateSince_(0),
      stableSince_(0) {}

ButtonEvent ButtonDebouncer::update(uint32_t now, bool rawPressed) {
  if (rawPressed != candidate_) {
    candidate_ = rawPressed;
    candidateSince_ = now;
  }

  if (candidate_ != stable_ && now - candidateSince_ >= debounceMs_) {
    stable_ = candidate_;
    stableSince_ = now;
    longPressSent_ = false;
    return stable_ ? ButtonEvent::Press : ButtonEvent::Release;
  }

  // The long press is reported while the button is still down, so the operator
  // gets the effect at the moment it is earned rather than on release.
  if (stable_ && !longPressSent_ && now - stableSince_ >= longPressMs_) {
    longPressSent_ = true;
    return ButtonEvent::LongPress;
  }

  return ButtonEvent::None;
}

}  // namespace Core
