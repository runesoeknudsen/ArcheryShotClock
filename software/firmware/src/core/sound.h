#pragma once

#include <cstdint>

#include "signals.h"

// Plays an Article 11.3 signal as a run of beeps.
//
// The count comes from the rulebook. The shape of each beep does not: Book 3
// specifies no tone, no duration and no gap, so those are configurable here
// with defaults chosen to be unmistakable on a field rather than to satisfy any
// rule. Officials count signals by ear, so the gap has to be long enough that
// two beeps are never heard as one.

namespace Core {

// Project defaults, not rulebook values.
constexpr uint16_t DEFAULT_BEEP_MS = 250;
constexpr uint16_t DEFAULT_GAP_MS = 200;

class SoundOutput {
public:
  virtual ~SoundOutput() = default;
  virtual void setActive(bool active) = 0;
};

class SoundController {
public:
  explicit SoundController(SoundOutput& output);

  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled_; }

  // Beep and gap length in milliseconds. Both must be non-zero.
  void setPattern(uint16_t beepMs, uint16_t gapMs);
  uint16_t beepMs() const { return beepMs_; }
  uint16_t gapMs() const { return gapMs_; }

  // Plays the number of signals Article 11.3 prescribes for this code.
  void playSignal(SignalCode code, uint32_t now);
  // Plays an explicit number of beeps.
  void play(uint8_t beeps, uint32_t now);

  void update(uint32_t now);
  void stop();

  bool isPlaying() const { return playing_; }
  // Beeps still to be sounded, including the one in progress.
  uint8_t remainingBeeps() const { return remainingBeeps_; }

private:
  void beginBeep(uint32_t now);

  SoundOutput& output_;
  uint16_t beepMs_;
  uint16_t gapMs_;
  uint32_t stepStartedAt_;
  uint8_t remainingBeeps_;
  bool enabled_;
  bool playing_;
  bool active_;
};

}  // namespace Core
