#pragma once

#include <Arduino.h>

#include "core/sound.h"

// Wired I2S amplifier (MAX98357A) that replaces the Bluetooth A2DP speaker.
//
// The chip is a DAC and Class-D amp in one: it wants a continuous I2S clock,
// 16-bit stereo slots, and a shutdown pin. Alerts are the same 880 Hz tone
// the Bluetooth path used to generate, scaled by a 0-100 volume setting.

class Max98357Output : public Core::SoundOutput {
public:
  Max98357Output();
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
