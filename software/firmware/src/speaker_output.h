#pragma once

#include <Arduino.h>

#include "core/sound.h"

// The field speaker uses the onboard ES8311 codec on the Waveshare board and
// an external MAX98357A on the original ESP32 board.
//
// Both paths play the same 880 Hz tone, scaled by a 0-100 volume setting.

class SpeakerOutput : public Core::SoundOutput {
public:
  SpeakerOutput();
  void begin();
  void update(uint32_t now);
  void setActive(bool active) override;
  void setVolume(uint8_t volume);
  uint8_t volume() const { return volume_; }
  // Plays a continuous tone for diagnosis, independent of the timer.
  void playTestTone(uint32_t now, uint32_t durationMs);
  bool isReady() const { return ready_; }

private:
  static void audioTaskThunk(void* context);
  void audioTask();
  void applyShutdown();

  volatile uint8_t volume_;
  volatile bool active_;
  volatile bool ready_;
  uint32_t toneUntil_;
};
