#include "settings.h"

#include <Preferences.h>

#include "config.h"
#include "core/volume.h"

namespace {
bool validMode(uint8_t value) { return value <= static_cast<uint8_t>(Core::Mode::Practice); }
bool validEventClass(uint8_t value) { return value <= static_cast<uint8_t>(Rules::EventClass::OtherReduced); }
bool validContent(uint8_t value) { return value <= static_cast<uint8_t>(Core::DisplayContent::Blank); }
}  // namespace

Settings SettingsStore::load() const {
  Preferences preferences;
  preferences.begin("archery", true);
  Settings settings{};
  settings.brightness = preferences.getUChar("brightness", Config::DEFAULT_BRIGHTNESS);
  settings.volume = preferences.getUChar("volume", Core::DEFAULT_VOLUME);
  settings.mode = preferences.getUChar("mode", static_cast<uint8_t>(Core::Mode::IndividualNonAlternating));
  settings.eventClass = preferences.getUChar("event_class", static_cast<uint8_t>(Rules::EventClass::Other));
  settings.arrowsPerEnd = preferences.getUChar("arrows", Rules::ARROWS_PER_END_SHORT);
  settings.displayContent = preferences.getUChar("display", static_cast<uint8_t>(Core::DisplayContent::Clock));
  settings.clockSeconds = preferences.getBool("clk_sec", true);
  settings.showAbcd = preferences.getBool("show_abcd", true);
  settings.abcdVertical = preferences.getBool("abcd_vert", true);
  settings.showEndLabels = preferences.getBool("end_lbl", true);
  settings.abcdFollowTimer = preferences.getBool("abcd_follow", false);
  settings.abcdColour = preferences.getUInt("abcd_col", 0xFFFFFFu);
  settings.replayOccupyOnResume = preferences.getBool("resume_10s", true);
  settings.firstShooter = preferences.getUChar("first", 1);
  settings.signalEachPeriod = preferences.getBool("sig_period", true);
  settings.abcdRotation = preferences.getBool("abcd", true);
  settings.details = preferences.getUChar("details", 2);
  settings.shootOff = preferences.getBool("shoot_off", false);
  settings.practiceMs = preferences.getUInt("practice_ms", 300000);
  settings.division = preferences.getUChar("division", static_cast<uint8_t>(Core::Division::Recurve));
  settings.matchLogic = preferences.getBool("match", false);
  settings.traceLevel = preferences.getUChar("trace", static_cast<uint8_t>(Core::TraceLevel::Off));
  settings.breakEnabled = preferences.getBool("brk_on", true);
  settings.breakAfterEnds = preferences.getUChar("brk_ends", 12);
  settings.breakMs = preferences.getUInt("brk_ms", 15 * 60 * 1000);
  settings.beepMs = preferences.getUShort("beep_ms", Core::DEFAULT_BEEP_MS);
  settings.gapMs = preferences.getUShort("gap_ms", Core::DEFAULT_GAP_MS);
  preferences.end();

  if (settings.brightness == 0) settings.brightness = Config::DEFAULT_BRIGHTNESS;
  settings.volume = Core::clampVolume(settings.volume);
  if (!validMode(settings.mode)) settings.mode = static_cast<uint8_t>(Core::Mode::IndividualNonAlternating);
  if (!validEventClass(settings.eventClass)) settings.eventClass = static_cast<uint8_t>(Rules::EventClass::Other);
  if (settings.arrowsPerEnd != Rules::ARROWS_PER_END_SHORT && settings.arrowsPerEnd != Rules::ARROWS_PER_END_LONG) {
    settings.arrowsPerEnd = Rules::ARROWS_PER_END_SHORT;
  }
  if (!validContent(settings.displayContent)) settings.displayContent = static_cast<uint8_t>(Core::DisplayContent::Clock);
  if (settings.firstShooter != 1 && settings.firstShooter != 2) settings.firstShooter = 1;
  if (settings.details < 1 || settings.details > 4) settings.details = 2;
  if (settings.practiceMs == 0) settings.practiceMs = 300000;
  if (settings.division > static_cast<uint8_t>(Core::Division::Compound)) {
    settings.division = static_cast<uint8_t>(Core::Division::Recurve);
  }
  if (settings.traceLevel > static_cast<uint8_t>(Core::TraceLevel::Verbose)) {
    settings.traceLevel = static_cast<uint8_t>(Core::TraceLevel::Off);
  }
  if (settings.breakAfterEnds > 36) settings.breakAfterEnds = 12;
  if (settings.breakEnabled && settings.breakMs == 0) settings.breakMs = 15 * 60 * 1000;
  if (settings.beepMs == 0) settings.beepMs = Core::DEFAULT_BEEP_MS;
  if (settings.gapMs == 0) settings.gapMs = Core::DEFAULT_GAP_MS;
  return settings;
}

void SettingsStore::save(const Settings& settings) const {
  Preferences preferences;
  preferences.begin("archery", false);
  preferences.putUChar("brightness", settings.brightness);
  preferences.putUChar("volume", settings.volume);
  preferences.putUChar("mode", settings.mode);
  preferences.putUChar("event_class", settings.eventClass);
  preferences.putUChar("arrows", settings.arrowsPerEnd);
  preferences.putUChar("display", settings.displayContent);
  preferences.putBool("clk_sec", settings.clockSeconds);
  preferences.putBool("show_abcd", settings.showAbcd);
  preferences.putBool("abcd_vert", settings.abcdVertical);
  preferences.putBool("end_lbl", settings.showEndLabels);
  preferences.putBool("abcd_follow", settings.abcdFollowTimer);
  preferences.putUInt("abcd_col", settings.abcdColour);
  preferences.putBool("resume_10s", settings.replayOccupyOnResume);
  preferences.putUChar("first", settings.firstShooter);
  preferences.putBool("sig_period", settings.signalEachPeriod);
  preferences.putBool("abcd", settings.abcdRotation);
  preferences.putUChar("details", settings.details);
  preferences.putBool("shoot_off", settings.shootOff);
  preferences.putUInt("practice_ms", settings.practiceMs);
  preferences.putUChar("division", settings.division);
  preferences.putBool("match", settings.matchLogic);
  preferences.putUChar("trace", settings.traceLevel);
  preferences.putBool("brk_on", settings.breakEnabled);
  preferences.putUChar("brk_ends", settings.breakAfterEnds);
  preferences.putUInt("brk_ms", settings.breakMs);
  preferences.putUShort("beep_ms", settings.beepMs);
  preferences.putUShort("gap_ms", settings.gapMs);
  preferences.end();
}

Core::SessionConfig sessionConfigFrom(const Settings& settings) {
  Core::SessionConfig config;
  config.mode = static_cast<Core::Mode>(settings.mode);
  config.eventClass = static_cast<Rules::EventClass>(settings.eventClass);
  config.arrowsPerEnd = settings.arrowsPerEnd;
  config.replayOccupyOnResume = settings.replayOccupyOnResume;
  config.firstShooter = settings.firstShooter;
  config.signalEachAlternatingPeriod = settings.signalEachPeriod;
  config.abcdRotation = settings.abcdRotation;
  config.details = settings.details;
  config.shootOff = settings.shootOff;
  config.practiceMs = settings.practiceMs;
  config.breakEnabled = settings.breakEnabled;
  config.breakAfterEnds = settings.breakAfterEnds;
  config.breakMs = settings.breakMs;
  return config;
}

Core::MatchConfig matchConfigFrom(const Settings& settings) {
  Core::MatchConfig config;
  config.division = static_cast<Core::Division>(settings.division);
  // Art. 12.1.4 ties the scoring system to the division, so it is derived
  // rather than offered as a separate switch that could contradict it.
  config.scoring = Core::defaultScoring(config.division);
  config.team = Rules::isTeam(static_cast<Core::Mode>(settings.mode));
  config.arrowsPerEnd = settings.arrowsPerEnd;
  return config;
}

void applySessionConfig(Settings& settings, const Core::SessionConfig& config) {
  settings.mode = static_cast<uint8_t>(config.mode);
  settings.eventClass = static_cast<uint8_t>(config.eventClass);
  settings.arrowsPerEnd = config.arrowsPerEnd;
  settings.replayOccupyOnResume = config.replayOccupyOnResume;
  settings.firstShooter = config.firstShooter;
  settings.signalEachPeriod = config.signalEachAlternatingPeriod;
  settings.abcdRotation = config.abcdRotation;
  settings.details = config.details;
  settings.shootOff = config.shootOff;
  settings.practiceMs = config.practiceMs;
  settings.breakEnabled = config.breakEnabled;
  settings.breakAfterEnds = config.breakAfterEnds;
  settings.breakMs = config.breakMs;
}
