#include "hal/buttons.h"

#include <esp_timer.h>

#include "config.h"

namespace {

constexpr uint8_t PINS[] = {Config::PIN_START, Config::PIN_STOP, Config::PIN_LINE_CLEAR, Config::PIN_NEXT_END,
                            Config::PIN_SUSPEND};
static_assert(sizeof(PINS) / sizeof(PINS[0]) == static_cast<uint8_t>(ButtonControl::Count),
              "every control needs a pin");

volatile bool emergencyLatched = false;
volatile uint32_t emergencyAtMs = 0;

void IRAM_ATTR emergencyIsr() {
  // esp_timer_get_time is safe from an interrupt; millis() is not guaranteed to
  // be. The timestamp is taken here so the log records when the button was
  // pressed, not when the main loop got round to noticing.
  emergencyAtMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
  emergencyLatched = true;
}

}  // namespace

const char* name(ButtonControl control) {
  switch (control) {
    case ButtonControl::Start: return "start";
    case ButtonControl::Stop: return "stop";
    case ButtonControl::LineClear: return "line_clear";
    case ButtonControl::NextEnd: return "next_end";
    case ButtonControl::Suspend: return "suspend";
    case ButtonControl::Count: break;
  }
  return "unknown";
}

void ButtonPanel::begin() {
  for (uint8_t index = 0; index < static_cast<uint8_t>(ButtonControl::Count); index++) {
    pinMode(PINS[index], INPUT_PULLUP);
  }
}

bool ButtonPanel::poll(uint32_t now, ButtonControl& control, bool& longPress) {
  const uint8_t count = static_cast<uint8_t>(ButtonControl::Count);
  for (uint8_t scanned = 0; scanned < count; scanned++) {
    const uint8_t index = cursor_;
    cursor_ = static_cast<uint8_t>((cursor_ + 1) % count);

    // Pulled up, so a closed switch reads low.
    const bool pressed = digitalRead(PINS[index]) == LOW;
    const Core::ButtonEvent event = buttons_[index].update(now, pressed);
    if (event == Core::ButtonEvent::Press || event == Core::ButtonEvent::LongPress) {
      control = static_cast<ButtonControl>(index);
      longPress = event == Core::ButtonEvent::LongPress;
      return true;
    }
  }
  return false;
}

void EmergencyStop::begin() {
  pinMode(Config::PIN_EMERGENCY, INPUT_PULLUP);
  // Wire the switch normally closed to ground: the pin sits low while all is
  // well, and goes high both when the button is pressed and when the wire
  // breaks. A broken emergency stop that stops the shooting is a nuisance; one
  // that silently does nothing is a hazard.
  attachInterrupt(digitalPinToInterrupt(Config::PIN_EMERGENCY), emergencyIsr, RISING);
}

bool EmergencyStop::taken(uint32_t& pressedAtMs) {
  noInterrupts();
  const bool latched = emergencyLatched;
  const uint32_t at = emergencyAtMs;
  emergencyLatched = false;
  interrupts();

  if (!latched) return false;
  pressedAtMs = at;
  return true;
}
