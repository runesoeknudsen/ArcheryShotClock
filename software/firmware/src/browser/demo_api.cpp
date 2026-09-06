#include "demo_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "core/display_logic.h"
#include "core/match_logic.h"
#include "core/shot_clock.h"
#include "core/sound.h"
#include "core/state_json.h"
#include "core/trace.h"
#include "core/volume.h"

namespace {

class LogSink : public Core::TraceSink {
public:
  static constexpr uint8_t CAPACITY = 32;
  static constexpr uint16_t LINE_BYTES = 240;

  void write(const char* line, uint16_t length) override {
    if (length == 0) return;
    uint16_t copy = length;
    while (copy > 0 && (line[copy - 1] == '\n' || line[copy - 1] == '\r')) copy--;
    if (copy > LINE_BYTES - 1) copy = LINE_BYTES - 1;
    for (uint16_t index = 0; index < copy; index++) lines_[next_][index] = line[index];
    lines_[next_][copy] = '\0';
    sequences_[next_] = sequenceOf(line, copy);
    next_ = static_cast<uint8_t>((next_ + 1) % CAPACITY);
    if (count_ < CAPACITY) count_++;
  }

  uint8_t count() const { return count_; }
  const char* line(uint8_t index) const {
    if (index >= count_) return "";
    const uint8_t oldest = count_ < CAPACITY ? 0 : next_;
    return lines_[(oldest + index) % CAPACITY];
  }
  uint32_t sequence(uint8_t index) const {
    if (index >= count_) return 0;
    const uint8_t oldest = count_ < CAPACITY ? 0 : next_;
    return sequences_[(oldest + index) % CAPACITY];
  }

private:
  static uint32_t sequenceOf(const char* line, uint16_t length) {
    const char* marker = "\"seq\":";
    for (uint16_t index = 0; index + 6 < length; index++) {
      bool matched = true;
      for (uint8_t offset = 0; offset < 6; offset++) {
        if (line[index + offset] != marker[offset]) {
          matched = false;
          break;
        }
      }
      if (!matched) continue;
      uint32_t value = 0;
      uint16_t cursor = index + 6;
      while (cursor < length && line[cursor] >= '0' && line[cursor] <= '9') {
        value = value * 10 + static_cast<uint32_t>(line[cursor] - '0');
        cursor++;
      }
      return value;
    }
    return 0;
  }

  char lines_[CAPACITY][LINE_BYTES] = {};
  uint32_t sequences_[CAPACITY] = {};
  uint8_t next_ = 0;
  uint8_t count_ = 0;
};

class BeepOutput : public Core::SoundOutput {
public:
  void setActive(bool active) override { active_ = active; }
  bool active() const { return active_; }

private:
  bool active_ = false;
};

const char* MODE_NAMES[] = {"PLAIN",      "IND_NONALT", "IND_ALT",   "TEAM_SIMUL",
                            "TEAM_ALT",   "MIXED_TEAM", "SHOOT_OFF", "PRACTICE"};
const char* CONTENT_NAMES[] = {"CLOCK", "CLOCK_END", "ARROWS", "SCORE", "SET_POINTS", "SHOOTER", "BLANK"};
const char* EVENT_CLASS_NAMES[] = {"ANNOUNCED", "OTHER", "OTHER_REDUCED"};
const char* DIVISION_NAMES[] = {"RECURVE", "BAREBOW", "COMPOUND"};

template <uint8_t Count>
bool lookup(const char* (&table)[Count], const char* value, uint8_t& index) {
  if (!value || !value[0]) return false;
  for (uint8_t candidate = 0; candidate < Count; candidate++) {
    if (std::strcmp(value, table[candidate]) == 0) {
      index = candidate;
      return true;
    }
  }
  return false;
}

int readInt(const char* body, const char* key, int fallback) {
  if (!body) return fallback;
  const char* found = std::strstr(body, key);
  if (!found) return fallback;
  const char* separator = std::strchr(found, ':');
  if (!separator) return fallback;
  return std::atoi(separator + 1);
}

bool readBool(const char* body, const char* key, bool fallback) {
  if (!body) return fallback;
  const char* found = std::strstr(body, key);
  if (!found) return fallback;
  const char* separator = std::strchr(found, ':');
  if (!separator) return fallback;
  if (std::strncmp(separator + 1, "true", 4) == 0) return true;
  if (std::strncmp(separator + 1, "false", 5) == 0) return false;
  return fallback;
}

void readString(const char* body, const char* key, char* out, size_t outSize) {
  out[0] = '\0';
  if (!body || outSize == 0) return;
  const char* found = std::strstr(body, key);
  if (!found) return;
  const char* separator = std::strchr(found, ':');
  if (!separator) return;
  const char* start = std::strchr(separator + 1, '"');
  if (!start) return;
  start++;
  const char* end = std::strchr(start, '"');
  if (!end) return;
  size_t length = static_cast<size_t>(end - start);
  if (length >= outSize) length = outSize - 1;
  std::memcpy(out, start, length);
  out[length] = '\0';
}

bool modeImplemented(Core::Mode mode) { return mode != Core::Mode::Plain && mode != Core::Mode::ShootOff; }

struct Host {
  LogSink log;
  Core::Tracer tracer;
  Core::ShotClock clock;
  Core::MatchLogic match;
  BeepOutput beeper;
  Core::SoundController sound;
  uint8_t brightness;
  uint8_t volume;
  bool clockSeconds;
  bool showAbcd;
  bool abcdVertical;
  bool showEndLabels;
  bool abcdFollowTimer;
  uint32_t abcdColour;
  uint8_t division;
  uint32_t now;
  uint32_t toneUntil;
  uint32_t pixels[DisplayLogic::PIXEL_COUNT];
  uint32_t logical[DisplayLogic::PIXEL_COUNT];
  char panelText[16];
  char stateJson[Core::STATE_JSON_BYTES];
  char logJson[8192];
  bool ready;

  Host()
      : tracer(log),
        clock(tracer),
        match(tracer),
        sound(beeper),
        brightness(16),
        volume(Core::DEFAULT_VOLUME),
        clockSeconds(true),
        showAbcd(true),
        abcdVertical(true),
        showEndLabels(true),
        abcdFollowTimer(false),
        abcdColour(DisplayLogic::COLOUR_WHITE),
        division(static_cast<uint8_t>(Core::Division::Recurve)),
        now(0),
        toneUntil(0),
        ready(false) {
    std::memset(pixels, 0, sizeof(pixels));
    std::memset(logical, 0, sizeof(logical));
    panelText[0] = '\0';
    stateJson[0] = '\0';
    logJson[0] = '\0';
  }
};

alignas(Host) unsigned char hostBuf[sizeof(Host)];
Host* hostPtr = nullptr;

Host& H() { return *hostPtr; }

void renderPanel() {
  const Core::StateSnapshot& snapshot = H().clock.snapshot();
  DisplayLogic::RenderRequest request;
  request.clockSeconds = H().clockSeconds;
  request.showAbcd = H().showAbcd;
  request.abcdVertical = H().abcdVertical;
  request.showEndLabels = H().showEndLabels;
  request.abcdFollowTimer = H().abcdFollowTimer;
  request.abcdColour = H().abcdColour;
  DisplayLogic::fillFromSnapshot(request, snapshot);
  const DisplayLogic::RenderResult result = DisplayLogic::renderFrame(request, H().pixels);
  std::strncpy(H().panelText, result.text, sizeof(H().panelText) - 1);
  H().panelText[sizeof(H().panelText) - 1] = '\0';

  for (uint8_t y = 0; y < DisplayLogic::ROWS; y++) {
    for (uint8_t x = 0; x < DisplayLogic::COLUMNS; x++) {
      H().logical[static_cast<uint16_t>(y) * DisplayLogic::COLUMNS + x] = H().pixels[DisplayLogic::ledIndex(x, y)];
    }
  }
}

Core::MatchConfig matchConfig() {
  Core::MatchConfig config;
  config.division = static_cast<Core::Division>(H().division);
  config.scoring = Core::defaultScoring(config.division);
  config.team = Rules::isTeam(H().clock.config().mode);
  config.arrowsPerEnd = H().clock.config().arrowsPerEnd;
  return config;
}

void sync(uint32_t now) {
  H().now = now;
  H().clock.setMatchTotals(H().match.state().runningTotal, H().match.state().setPoints);
  Core::SignalCode signal = Core::SignalCode::None;
  while (H().clock.takeSignal(signal)) H().sound.playSignal(signal, now);
  H().sound.update(now);
  if (H().toneUntil && now >= H().toneUntil) {
    H().toneUntil = 0;
    if (!H().sound.isPlaying()) H().beeper.setActive(false);
  } else if (H().toneUntil && now < H().toneUntil) {
    H().beeper.setActive(true);
  }
  renderPanel();
}

}  // namespace

void demo_init(uint32_t now_ms) {
  if (hostPtr) {
    hostPtr->~Host();
    hostPtr = nullptr;
  }
  hostPtr = new (hostBuf) Host();
  H().tracer.setLevel(Core::TraceLevel::Off);
  H().tracer.boot(now_ms, "esp-archery-timer 0.2.0-browser");
  H().clock.configure(now_ms, Core::SessionConfig{});
  H().match.configure(now_ms, matchConfig());
  H().match.enable(now_ms, false);
  H().sound.setPattern(Core::DEFAULT_BEEP_MS, Core::DEFAULT_GAP_MS);
  H().ready = true;
  sync(now_ms);
}

void demo_tick(uint32_t now_ms) {
  if (!H().ready) return;
  H().clock.update(now_ms);
  sync(now_ms);
}

int demo_control(const char* action, int32_t arg) {
  if (!H().ready || !action) return 1;
  const uint32_t now = H().now;
  H().tracer.input(now, "web", action, 0);
  if (std::strcmp(action, "start") == 0) {
    H().clock.start(now);
  } else if (std::strcmp(action, "stop") == 0) {
    H().clock.stop(now);
  } else if (std::strcmp(action, "line_clear") == 0) {
    H().clock.lineClear(now);
  } else if (std::strcmp(action, "next_end") == 0) {
    if (H().match.isEnabled()) H().match.completeEnd(now);
    H().clock.nextEnd(now);
  } else if (std::strcmp(action, "reset_end") == 0) {
    H().clock.resetEnd(now);
  } else if (std::strcmp(action, "suspend") == 0) {
    H().clock.suspend(now);
  } else if (std::strcmp(action, "resume") == 0) {
    H().clock.resume(now);
  } else if (std::strcmp(action, "add_arrow") == 0) {
    H().clock.addArrow(now);
  } else if (std::strcmp(action, "remove_arrow") == 0) {
    H().clock.removeArrow(now);
  } else if (std::strcmp(action, "emergency") == 0) {
    H().clock.emergency(now);
  } else if (std::strcmp(action, "clear_emergency") == 0) {
    H().clock.clearEmergency(now);
  } else if (std::strcmp(action, "extend") == 0) {
    if (arg > 0) H().clock.extendTime(now, static_cast<uint32_t>(arg) * 1000UL);
  } else {
    return 1;
  }
  sync(now);
  return 0;
}

int demo_session(const char* json) {
  if (!H().ready) return 1;
  const uint32_t now = H().now;
  H().tracer.input(now, "web", "session", 0);
  Core::SessionConfig config = H().clock.config();
  uint8_t index = 0;
  char text[32] = {};

  readString(json, "mode", text, sizeof(text));
  if (lookup(MODE_NAMES, text, index)) {
    const Core::Mode requested = static_cast<Core::Mode>(index);
    if (!modeImplemented(requested)) return 2;
    config.mode = requested;
  }

  readString(json, "eventClass", text, sizeof(text));
  if (lookup(EVENT_CLASS_NAMES, text, index)) config.eventClass = static_cast<Rules::EventClass>(index);

  const int arrows = readInt(json, "arrowsPerEnd", 0);
  if (arrows == Rules::ARROWS_PER_END_SHORT || arrows == Rules::ARROWS_PER_END_LONG) {
    config.arrowsPerEnd = static_cast<uint8_t>(arrows);
  }

  const int firstShooter = readInt(json, "firstShooter", 0);
  if (firstShooter == 1 || firstShooter == 2) config.firstShooter = static_cast<uint8_t>(firstShooter);

  const int details = readInt(json, "details", 0);
  if (details >= 1 && details <= 4) config.details = static_cast<uint8_t>(details);

  const int practiceSeconds = readInt(json, "practiceSeconds", 0);
  if (practiceSeconds > 0) config.practiceMs = static_cast<uint32_t>(practiceSeconds) * 1000UL;

  readString(json, "division", text, sizeof(text));
  if (lookup(DIVISION_NAMES, text, index)) H().division = index;

  config.replayOccupyOnResume = readBool(json, "resumeOccupy", config.replayOccupyOnResume);
  config.signalEachAlternatingPeriod = readBool(json, "signalEachPeriod", config.signalEachAlternatingPeriod);
  config.abcdRotation = readBool(json, "abcdRotation", config.abcdRotation);
  config.shootOff = readBool(json, "shootOff", config.shootOff);
  config.breakEnabled = readBool(json, "breakEnabled", config.breakEnabled);
  if (json && std::strstr(json, "breakAfterEnds")) {
    const int breakAfterEnds = readInt(json, "breakAfterEnds", config.breakAfterEnds);
    if (breakAfterEnds >= 0 && breakAfterEnds <= 36) {
      config.breakAfterEnds = static_cast<uint8_t>(breakAfterEnds);
    }
  }
  if (json && std::strstr(json, "breakSeconds")) {
    const int breakSeconds = readInt(json, "breakSeconds", 0);
    if (breakSeconds > 0) config.breakMs = static_cast<uint32_t>(breakSeconds) * 1000UL;
  }

  H().clock.configure(now, config);
  H().match.configure(now, matchConfig());
  H().match.enable(now, readBool(json, "matchLogic", H().match.isEnabled()));
  sync(now);
  return 0;
}

int demo_display(const char* content) {
  if (!H().ready) return 1;
  uint8_t index = 0;
  if (!lookup(CONTENT_NAMES, content, index)) return 1;
  const Core::DisplayContent requested = static_cast<Core::DisplayContent>(index);
  if (!DisplayLogic::contentAvailable(requested)) return 2;
  H().clock.setDisplayContent(H().now, requested);
  sync(H().now);
  return 0;
}

int demo_clock_seconds(int enabled) {
  if (!H().ready) return 1;
  const bool clockSeconds = enabled != 0;
  H().tracer.config(H().now, "clock_seconds", H().clockSeconds ? 1 : 0, clockSeconds ? 1 : 0);
  H().clockSeconds = clockSeconds;
  sync(H().now);
  return 0;
}

int demo_panel_options(const char* json) {
  if (!H().ready) return 1;
  H().clockSeconds = readBool(json, "clockSeconds", H().clockSeconds);
  H().showAbcd = readBool(json, "showAbcd", H().showAbcd);
  H().abcdVertical = readBool(json, "abcdVertical", H().abcdVertical);
  H().showEndLabels = readBool(json, "showEndLabels", H().showEndLabels);
  H().abcdFollowTimer = readBool(json, "abcdFollowTimer", H().abcdFollowTimer);
  char colour[16] = {};
  readString(json, "abcdColour", colour, sizeof(colour));
  if (colour[0] != '\0') {
    H().abcdColour = DisplayLogic::parseCssColour(colour, H().abcdColour);
  }
  sync(H().now);
  return 0;
}

int demo_brightness(int brightness) {
  if (!H().ready) return 1;
  if (brightness < 1 || brightness > 255) return 1;
  H().tracer.config(H().now, "brightness", H().brightness, brightness);
  H().brightness = static_cast<uint8_t>(brightness);
  return 0;
}

int demo_sound(const char* json) {
  if (!H().ready) return 1;
  const int beep = readInt(json, "beepMs", 0);
  const int gap = readInt(json, "gapMs", 0);
  if (beep > 0 && gap > 0) {
    H().tracer.config(H().now, "beep_ms", H().sound.beepMs(), beep);
    H().tracer.config(H().now, "gap_ms", H().sound.gapMs(), gap);
    H().sound.setPattern(static_cast<uint16_t>(beep), static_cast<uint16_t>(gap));
  }
  if (json && std::strstr(json, "volume")) {
    const uint8_t volume = Core::clampVolume(readInt(json, "volume", H().volume));
    H().tracer.config(H().now, "volume", H().volume, volume);
    H().volume = volume;
  }
  return 0;
}

void demo_test_tone(uint32_t now_ms, uint32_t duration_ms) {
  if (!H().ready) return;
  H().now = now_ms;
  H().toneUntil = now_ms + duration_ms;
  H().beeper.setActive(true);
}

int demo_score(const char* json) {
  if (!H().ready) return 1;
  if (!H().match.isEnabled()) return 2;
  const uint32_t now = H().now;
  char action[24] = {};
  readString(json, "action", action, sizeof(action));
  const int side = readInt(json, "side", -1);
  bool accepted = true;

  if (std::strcmp(action, "arrow") == 0) {
    char value[8] = {};
    readString(json, "value", value, sizeof(value));
    accepted = H().match.recordArrow(now, static_cast<uint8_t>(side),
                                     std::strcmp(value, "X") == 0 ? Core::ARROW_X
                                                                  : static_cast<uint8_t>(readInt(json, "value", 0)));
  } else if (std::strcmp(action, "undo") == 0) {
    accepted = H().match.removeLastArrow(now, static_cast<uint8_t>(side));
  } else if (std::strcmp(action, "forfeit") == 0) {
    char article[12] = {};
    readString(json, "article", article, sizeof(article));
    accepted = H().match.forfeitHighest(now, static_cast<uint8_t>(side),
                                        std::strcmp(article, "13.6.2") == 0 ? "13.6.2" : "13.3");
  } else if (std::strcmp(action, "yellow") == 0) {
    H().match.yellowCard(now, static_cast<uint8_t>(side));
  } else if (std::strcmp(action, "clear_yellow") == 0) {
    H().match.clearYellowCard(now, static_cast<uint8_t>(side));
  } else if (std::strcmp(action, "warn") == 0) {
    H().match.warn(now, static_cast<uint8_t>(side));
  } else if (std::strcmp(action, "disqualify") == 0) {
    H().match.disqualify(now, static_cast<uint8_t>(side));
  } else if (std::strcmp(action, "complete_end") == 0) {
    H().match.completeEnd(now);
  } else if (std::strcmp(action, "reset") == 0) {
    H().match.reset(now);
  } else {
    return 1;
  }
  if (!accepted) return 2;
  sync(now);
  return 0;
}

int demo_trace_level(const char* level) {
  if (!H().ready) return 1;
  Core::TraceLevel parsed = H().tracer.level();
  if (!Core::parseTraceLevel(level, parsed)) return 1;
  H().tracer.setLevel(parsed);
  H().tracer.configText(H().now, "trace_level", "", Core::name(parsed));
  return 0;
}

const char* demo_state_json(void) {
  if (!H().ready) return "{}";
  Core::StateView view;
  view.snapshot = &H().clock.snapshot();
  view.session = &H().clock.config();
  view.match = &H().match.state();
  view.matchConfig = &H().match.config();
  view.matchEnabled = H().match.isEnabled();
  view.perArrowMs = H().clock.perArrowMs();
  view.brightness = H().brightness;
  view.clockSeconds = H().clockSeconds;
  view.showAbcd = H().showAbcd;
  view.abcdVertical = H().abcdVertical;
  view.showEndLabels = H().showEndLabels;
  view.abcdFollowTimer = H().abcdFollowTimer;
  view.abcdColour = H().abcdColour;
  view.beepMs = H().sound.beepMs();
  view.gapMs = H().sound.gapMs();
  view.soundEnabled = H().sound.isEnabled();
  view.volume = H().volume;
  view.panelText = H().panelText;
  view.traceLevel = H().tracer.level();
  view.traceSeq = H().tracer.sequence();
  view.freeHeap = 0;
  view.minFreeHeap = 0;
  view.largestFreeBlock = 0;
  view.apClients = 1;
  Core::renderStateJson(view, H().stateJson, sizeof(H().stateJson));
  return H().stateJson;
}

const char* demo_log_json(uint32_t after_seq) {
  if (!H().ready) return "{\"lines\":[]}";
  char* out = H().logJson;
  size_t used = 0;
  const size_t cap = sizeof(H().logJson);
  auto append = [&](const char* text) {
    const size_t length = std::strlen(text);
    if (used + length + 1 >= cap) return;
    std::memcpy(out + used, text, length);
    used += length;
    out[used] = '\0';
  };
  append("{\"lines\":[");
  bool first = true;
  for (uint8_t index = 0; index < H().log.count(); index++) {
    if (H().log.sequence(index) <= after_seq) continue;
    if (!first) append(",");
    first = false;
    append(H().log.line(index));
  }
  append("]}");
  return H().logJson;
}

uint16_t demo_panel_columns(void) { return DisplayLogic::COLUMNS; }
uint16_t demo_panel_rows(void) { return DisplayLogic::ROWS; }
const uint32_t* demo_logical_pixels(void) { return hostPtr && H().ready ? H().logical : nullptr; }
uint32_t demo_pixel(uint16_t index) {
  if (!hostPtr || !H().ready || index >= DisplayLogic::PIXEL_COUNT) return 0;
  return H().logical[index];
}
int demo_sound_active(void) { return (H().ready && H().beeper.active()) ? 1 : 0; }
