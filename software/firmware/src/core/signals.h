#pragma once

#include <cstdint>

#include "rules.h"

// The sound-signal vocabulary of World Archery Book 3, Article 11.3.
//
// The rulebook defines the *number* of signals for each meaning and nothing
// else. It never names a device - no whistle, horn, buzzer or bell appears in
// the text - and gives no tone, pitch, volume, beep length or gap between
// beeps. So the counts below are rules, and everything about how they sound is
// a project decision made in sound.h and config.h.

namespace Core {

enum class SignalCode : uint8_t { None, OccupyLine, Start, Stop, Scoring, Resume, Emergency };

const char* name(SignalCode code);

// The number of sound signals Article 11.3 prescribes. Emergency is a minimum
// ("a series of at least 5 sound signals"), the rest are exact.
uint8_t signalCount(SignalCode code);

}  // namespace Core
