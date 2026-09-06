#pragma once

#include <cstdint>

// Project volume, not a rulebook value. Article 11.3 fixes how many signals
// are given and says nothing about how loud they are, so the director sets
// this from the web UI and it is applied to the MAX98357A PCM path.

namespace Core {

constexpr uint8_t DEFAULT_VOLUME = 60;
constexpr uint8_t MIN_VOLUME = 0;
constexpr uint8_t MAX_VOLUME = 100;

inline uint8_t clampVolume(int value) {
  if (value < static_cast<int>(MIN_VOLUME)) return MIN_VOLUME;
  if (value > static_cast<int>(MAX_VOLUME)) return MAX_VOLUME;
  return static_cast<uint8_t>(value);
}

// Scales a 16-bit PCM sample by a 0-100 volume. Zero is silence.
inline int16_t scaleSample(int16_t sample, uint8_t volume) {
  if (volume == 0) return 0;
  if (volume >= MAX_VOLUME) return sample;
  return static_cast<int16_t>((static_cast<int32_t>(sample) * volume) / MAX_VOLUME);
}

}  // namespace Core
