#include "sound.h"

namespace Core {

SoundController::SoundController(SoundOutput& output)
    : output_(output),
      beepMs_(DEFAULT_BEEP_MS),
      gapMs_(DEFAULT_GAP_MS),
      stepStartedAt_(0),
      remainingBeeps_(0),
      enabled_(true),
      playing_(false),
      active_(false) {}

void SoundController::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled_) stop();
}

void SoundController::setPattern(uint16_t beepMs, uint16_t gapMs) {
  if (beepMs > 0) beepMs_ = beepMs;
  if (gapMs > 0) gapMs_ = gapMs;
}

void SoundController::playSignal(SignalCode code, uint32_t now) { play(signalCount(code), now); }

void SoundController::play(uint8_t beeps, uint32_t now) {
  if (!enabled_ || beeps == 0) return;
  remainingBeeps_ = beeps;
  playing_ = true;
  beginBeep(now);
}

void SoundController::beginBeep(uint32_t now) {
  stepStartedAt_ = now;
  active_ = true;
  output_.setActive(true);
}

void SoundController::update(uint32_t now) {
  if (!playing_) return;

  while (playing_) {
    const uint16_t stepMs = active_ ? beepMs_ : gapMs_;
    if (now - stepStartedAt_ < stepMs) return;
    stepStartedAt_ += stepMs;

    if (active_) {
      // The beep just finished. Fall silent, and stop altogether if that was
      // the last one - a trailing gap would delay the next signal for nothing.
      active_ = false;
      output_.setActive(false);
      remainingBeeps_--;
      if (remainingBeeps_ == 0) {
        playing_ = false;
        return;
      }
    } else {
      beginBeep(stepStartedAt_);
    }
  }
}

void SoundController::stop() {
  playing_ = false;
  active_ = false;
  remainingBeeps_ = 0;
  output_.setActive(false);
}

}  // namespace Core
