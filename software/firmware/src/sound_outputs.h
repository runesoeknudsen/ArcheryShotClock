#pragma once

#include "core/sound.h"
#include "max98357.h"

class ActiveBuzzerOutput : public Core::SoundOutput {
public:
  void begin();
  void setActive(bool active) override;
};

// The box buzzer and the field amplifier sound together. The MAX98357A is
// wired, so there is no pairing step and no "use the buzzer until the speaker
// connects" fallback the way there was with Bluetooth.
class CombinedSoundOutput : public Core::SoundOutput {
public:
  CombinedSoundOutput(ActiveBuzzerOutput& buzzer, Max98357Output& amplifier);
  void setActive(bool active) override;

private:
  ActiveBuzzerOutput& buzzer_;
  Max98357Output& amplifier_;
};
