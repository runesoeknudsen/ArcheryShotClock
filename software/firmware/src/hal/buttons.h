#pragma once

#include <Arduino.h>

#include "core/button_logic.h"

// The physical console.
//
// Every control here is also on the web UI, with one exception that matters:
// the emergency stop. Article 11.3.3 is the one signal that must reach the
// field when everything else has failed, so it runs from an interrupt and
// touches neither Wi-Fi nor the web server on its way.

enum class ButtonControl : uint8_t { Start, Stop, LineClear, NextEnd, Suspend, Count };

const char* name(ButtonControl control);

class ButtonPanel {
public:
  void begin();

  // Returns one event per call; call until it returns false. longPress marks a
  // deliberate hold, used to separate a conceded match from an ordinary stop.
  bool poll(uint32_t now, ButtonControl& control, bool& longPress);

private:
  Core::ButtonDebouncer buttons_[static_cast<uint8_t>(ButtonControl::Count)];
  uint8_t cursor_ = 0;
};

// Latches the emergency press in the interrupt handler and hands it to the main
// loop with the timestamp it actually happened at, so the record is accurate
// even though the trace is written a tick later. Tracing from inside an ISR
// would risk the very stall it is meant to survive.
class EmergencyStop {
public:
  void begin();
  // True once per press; fills pressedAtMs with the ISR's own timestamp.
  bool taken(uint32_t& pressedAtMs);
};
