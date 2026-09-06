#include "sound_outputs.h"

#include <Arduino.h>

#include "config.h"

void ActiveBuzzerOutput::begin() {
  pinMode(Config::SOUND_PIN, OUTPUT);
  digitalWrite(Config::SOUND_PIN, LOW);
}

void ActiveBuzzerOutput::setActive(bool active) { digitalWrite(Config::SOUND_PIN, active ? HIGH : LOW); }

CombinedSoundOutput::CombinedSoundOutput(ActiveBuzzerOutput& buzzer, Max98357Output& amplifier)
    : buzzer_(buzzer), amplifier_(amplifier) {}

void CombinedSoundOutput::setActive(bool active) {
  buzzer_.setActive(active);
  amplifier_.setActive(active);
}
