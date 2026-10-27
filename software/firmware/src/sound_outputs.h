#pragma once

#include "core/sound.h"
#include "speaker_output.h"

class ActiveBuzzerOutput : public Core::SoundOutput {
public:
  void begin();
  void setActive(bool active) override;
};

// The box buzzer and the board-specific wired speaker sound together.
class CombinedSoundOutput : public Core::SoundOutput {
public:
  CombinedSoundOutput(ActiveBuzzerOutput& buzzer, SpeakerOutput& speaker);
  void setActive(bool active) override;

private:
  ActiveBuzzerOutput& buzzer_;
  SpeakerOutput& speaker_;
};
