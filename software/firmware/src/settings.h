#pragma once

#include <Arduino.h>

#include "core/rules.h"
#include "core/match_logic.h"
#include "core/shot_clock.h"
#include "core/sound.h"
#include "core/snapshot.h"

// Everything that must survive a reboot. The session settings matter as much
// as the hardware ones: a director who has set up a match should not have to
// set it up again because the battery was swapped between ends.
struct Settings {
  uint8_t brightness;
  uint8_t volume;
  bool clockSeconds;
  bool showAbcd;
  bool abcdVertical;
  bool showEndLabels;
  bool abcdFollowTimer;
  uint32_t abcdColour;

  uint8_t mode;             // Core::Mode
  uint8_t eventClass;       // Rules::EventClass
  uint8_t arrowsPerEnd;     // Art. 10.1: three or six
  uint8_t displayContent;   // Core::DisplayContent
  bool replayOccupyOnResume;
  uint8_t firstShooter;     // Art. 11.1.4.1 / 11.1.4.3
  bool signalEachPeriod;    // Art. 11.3.4
  bool abcdRotation;        // Art. 11.2.3.1, default on
  uint8_t details;
  bool shootOff;            // Art. 12.5
  uint32_t practiceMs;      // Chapter 14
  uint8_t division;         // Core::Division, Art. 12.1.4
  bool matchLogic;
  // Off during an event. Testing mode writes the evidence trail.
  uint8_t traceLevel;       // Core::TraceLevel

  bool breakEnabled;
  uint8_t breakAfterEnds;
  uint32_t breakMs;

  // Not rulebook values - Article 11.3 defines signal counts only.
  uint16_t beepMs;
  uint16_t gapMs;
};

class SettingsStore {
public:
  Settings load() const;
  void save(const Settings& settings) const;
};

// Converts stored bytes into the session configuration, clamping anything out
// of range back to a legal value rather than trusting flash.
Core::SessionConfig sessionConfigFrom(const Settings& settings);
Core::MatchConfig matchConfigFrom(const Settings& settings);
void applySessionConfig(Settings& settings, const Core::SessionConfig& config);
