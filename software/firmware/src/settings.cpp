#include "settings.h"

#include <Preferences.h>

#include "config.h"
#include "core/display_geometry.h"
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
  settings.panelPreset = preferences.getUChar("preset", static_cast<uint8_t>(DisplayLogic::DEFAULT_PRESET));
  settings.orientation = preferences.getUChar("orient", static_cast<uint8_t>(DisplayLogic::Orientation::Landscape));
  settings.lineCount = preferences.getUChar("nlines", 0);
  const size_t lineBytes = preferences.getBytes("lines", settings.lines, sizeof(settings.lines));
  if (lineBytes < sizeof(settings.lines)) {
    for (uint8_t index = 0; index < DisplayLogic::MAX_CONTENT_LINES; index++) settings.lines[index] = 0;
  }
  settings.lineScale = preferences.getUChar("lscale", static_cast<uint8_t>(DisplayLogic::LineScaleMode::Fill));
  settings.heroLine = preferences.getUChar("hero", 0);
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
  settings.waves = preferences.getUChar("waves", 0);
  settings.shootOff = preferences.getBool("shoot_off", false);
  settings.practiceMs = preferences.getUInt("practice_ms", 300000);
  settings.division = preferences.getUChar("division", static_cast<uint8_t>(Core::Division::Recurve));
  settings.matchLogic = preferences.getBool("match", false);
  settings.traceLevel = preferences.getUChar("trace", static_cast<uint8_t>(Core::TraceLevel::Off));
  settings.breakEnabled = preferences.getBool("brk_on", true);
  settings.breakAfterEnds = preferences.getUChar("brk_ends", 12);
  settings.breakMs = preferences.getUInt("brk_ms", 15 * 60 * 1000);
  settings.endsPerRound = preferences.getUChar("ends_rnd", settings.breakAfterEnds);
  settings.qualificationRounds = preferences.getUChar("qual_rnds", 2);
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
  if (!DisplayLogic::validPreset(settings.panelPreset)) {
    settings.panelPreset = static_cast<uint8_t>(DisplayLogic::DEFAULT_PRESET);
  }
  if (!DisplayLogic::validOrientation(settings.orientation)) {
    settings.orientation = static_cast<uint8_t>(DisplayLogic::Orientation::Landscape);
  }
  {
    const DisplayLogic::Geometry geometry = DisplayLogic::geometryFor(
        static_cast<DisplayLogic::PanelPreset>(settings.panelPreset),
        static_cast<DisplayLogic::Orientation>(settings.orientation));
    const uint8_t maxLines = DisplayLogic::maxLinesFor(geometry);
    if (settings.lineCount == 0 || settings.lineCount > maxLines) {
      Core::DisplayContent lines[DisplayLogic::MAX_CONTENT_LINES] = {};
      settings.lineCount = DisplayLogic::defaultLines(geometry, lines, maxLines);
      for (uint8_t index = 0; index < settings.lineCount; index++) {
        settings.lines[index] = static_cast<uint8_t>(lines[index]);
      }
    }
    for (uint8_t index = 0; index < settings.lineCount; index++) {
      if (!validContent(settings.lines[index])) {
        settings.lines[index] = static_cast<uint8_t>(Core::DisplayContent::Clock);
      }
    }
    settings.displayContent = settings.lines[0];
    if (!DisplayLogic::validLineScale(settings.lineScale)) {
      settings.lineScale = static_cast<uint8_t>(DisplayLogic::LineScaleMode::Fill);
    }
    if (settings.heroLine >= settings.lineCount) settings.heroLine = 0;
  }
  if (settings.firstShooter != 1 && settings.firstShooter != 2) settings.firstShooter = 1;
  if (settings.details < 1 || settings.details > 4) settings.details = 2;
  if (settings.waves > 3) settings.waves = 0;
  if (settings.practiceMs == 0) settings.practiceMs = 300000;
  if (settings.division > static_cast<uint8_t>(Core::Division::Compound)) {
    settings.division = static_cast<uint8_t>(Core::Division::Recurve);
  }
  if (settings.traceLevel > static_cast<uint8_t>(Core::TraceLevel::Verbose)) {
    settings.traceLevel = static_cast<uint8_t>(Core::TraceLevel::Off);
  }
  if (settings.breakAfterEnds > 36) settings.breakAfterEnds = 12;
  if (settings.endsPerRound < 1 || settings.endsPerRound > 36) {
    settings.endsPerRound = settings.breakAfterEnds > 0 ? settings.breakAfterEnds : 12;
  }
  if (settings.qualificationRounds < 1 || settings.qualificationRounds > 8) {
    settings.qualificationRounds = 2;
  }
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
  preferences.putUChar("preset", settings.panelPreset);
  preferences.putUChar("orient", settings.orientation);
  preferences.putUChar("nlines", settings.lineCount);
  preferences.putBytes("lines", settings.lines, sizeof(settings.lines));
  preferences.putUChar("lscale", settings.lineScale);
  preferences.putUChar("hero", settings.heroLine);
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
  preferences.putUChar("waves", settings.waves);
  preferences.putBool("shoot_off", settings.shootOff);
  preferences.putUInt("practice_ms", settings.practiceMs);
  preferences.putUChar("division", settings.division);
  preferences.putBool("match", settings.matchLogic);
  preferences.putUChar("trace", settings.traceLevel);
  preferences.putBool("brk_on", settings.breakEnabled);
  preferences.putUChar("brk_ends", settings.breakAfterEnds);
  preferences.putUInt("brk_ms", settings.breakMs);
  preferences.putUChar("ends_rnd", settings.endsPerRound);
  preferences.putUChar("qual_rnds", settings.qualificationRounds);
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
  config.waves = settings.waves;
  config.shootOff = settings.shootOff;
  config.practiceMs = settings.practiceMs;
  config.breakEnabled = settings.breakEnabled;
  config.breakAfterEnds = settings.breakAfterEnds;
  config.breakMs = settings.breakMs;
  config.endsPerRound = settings.endsPerRound;
  config.qualificationRounds = settings.qualificationRounds;
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
  settings.waves = config.waves;
  settings.shootOff = config.shootOff;
  settings.practiceMs = config.practiceMs;
  settings.breakEnabled = config.breakEnabled;
  settings.breakAfterEnds = config.breakAfterEnds;
  settings.breakMs = config.breakMs;
  settings.endsPerRound = config.endsPerRound;
  settings.qualificationRounds = config.qualificationRounds;
}
