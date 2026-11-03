#include "sound_outputs.h"

#include <Arduino.h>

#include "config.h"

void ActiveBuzzerOutput::begin() {
  pinMode(Config::SOUND_PIN, OUTPUT);
  digitalWrite(Config::SOUND_PIN, LOW);
}

void ActiveBuzzerOutput::setActive(bool active) { digitalWrite(Config::SOUND_PIN, active ? HIGH : LOW); }

CombinedSoundOutput::CombinedSoundOutput(ActiveBuzzerOutput& buzzer, SpeakerOutput& speaker)
    : buzzer_(buzzer), speaker_(speaker) {}

void CombinedSoundOutput::setActive(bool active) {
  buzzer_.setActive(active);
  speaker_.setActive(active);
}
