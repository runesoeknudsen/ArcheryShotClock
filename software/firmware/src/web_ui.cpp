#include "web_ui.h"

#include "config.h"
#include <esp_heap_caps.h>
#include <WiFi.h>

#include "core/display_logic.h"
#include "core/volume.h"
#include "web_page.h"

namespace {

const char* MODE_NAMES[] = {"PLAIN",      "IND_NONALT", "IND_ALT",   "TEAM_SIMUL",
                            "TEAM_ALT",   "MIXED_TEAM", "SHOOT_OFF", "PRACTICE"};
const char* CONTENT_NAMES[] = {"CLOCK", "CLOCK_END", "ARROWS", "SCORE", "SET_POINTS", "SHOOTER", "BLANK"};
const char* EVENT_CLASS_NAMES[] = {"ANNOUNCED", "OTHER", "OTHER_REDUCED"};
const char* DIVISION_NAMES[] = {"RECURVE", "BAREBOW", "COMPOUND"};

template <uint8_t Count>
bool lookup(const char* (&table)[Count], const String& value, uint8_t& index) {
  for (uint8_t candidate = 0; candidate < Count; candidate++) {
    if (value == table[candidate]) {
      index = candidate;
      return true;
    }
  }
  return false;
}

// PLAIN is the retired pre-rulebook countdown, and a shoot-off is a flag on
// the session rather than a mode of its own, because Art. 11.2.1 applies the
// same per-arrow rates to shoot-offs as to the round they belong to.
bool modeImplemented(Core::Mode mode) { return mode != Core::Mode::Plain && mode != Core::Mode::ShootOff; }

}  // namespace

WebUi::WebUi(Settings& settings, SettingsStore& store, Core::ShotClock& clock, Core::MatchLogic& match,
             Core::SoundController& sound, Max98357Output& amplifier, Core::Tracer& tracer, LogBufferSink& log)
    : settings_(settings),
      store_(store),
      clock_(clock),
      match_(match),
      sound_(sound),
      amplifier_(amplifier),
      tracer_(tracer),
      log_(log) {
  panelText_[0] = '\0';
}

void WebUi::setPanelText(const char* text) {
  uint8_t index = 0;
  while (text[index] != '\0' && index < sizeof(panelText_) - 1) {
    panelText_[index] = text[index];
    index++;
  }
  panelText_[index] = '\0';
}

void WebUi::traceInput(const char* control) { tracer_.input(now_, "web", control, 0); }

void WebUi::begin() {
  server_.on("/", HTTP_GET, [this]() { sendPage(PAGE_GZ, PAGE_GZ_LEN); });
  server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
  server_.on("/api/log", HTTP_GET, [this]() { handleLog(); });
  server_.on("/api/control", HTTP_POST, [this]() { handleControl(); });
  server_.on("/api/session", HTTP_POST, [this]() { handleSession(); });
  server_.on("/api/display", HTTP_POST, [this]() { handleDisplay(); });
  server_.on("/api/brightness", HTTP_POST, [this]() { handleBrightness(); });
  server_.on("/api/sound", HTTP_POST, [this]() { handleSound(); });
  server_.on("/api/sound/test", HTTP_POST, [this]() { handleSoundTest(); });
  server_.on("/api/score", HTTP_POST, [this]() { handleScore(); });
  server_.on("/api/trace", HTTP_POST, [this]() { handleTrace(); });

  server_.begin();
}

void WebUi::sendPage(const uint8_t* page, size_t length) {
  // Streamed from flash with an explicit length. Copying the page into a
  // String first would ask for a 20 KB allocation on a board already running
  // an access point, and a failed or partial send arrives as a document
  // truncated somewhere in the middle - which looks like a broken page
  // rather than the memory error it is.
  server_.sendHeader("Content-Encoding", "gzip");
  // Without this, a browser that cached the previous firmware's page keeps
  // serving it after an update and the new controls simply never appear.
  server_.sendHeader("Cache-Control", "no-cache");
  server_.send_P(200, "text/html", reinterpret_cast<PGM_P>(page), length);
}

void WebUi::handleClient(uint32_t now) {
  now_ = now;
  server_.handleClient();
}

void WebUi::handleState() {
  Core::StateView view;
  view.snapshot = &clock_.snapshot();
  view.session = &clock_.config();
  view.match = &match_.state();
  view.matchConfig = &match_.config();
  view.matchEnabled = match_.isEnabled();
  view.perArrowMs = clock_.perArrowMs();
  view.brightness = settings_.brightness;
  view.clockSeconds = settings_.clockSeconds;
  view.showAbcd = settings_.showAbcd;
  view.abcdVertical = settings_.abcdVertical;
  view.showEndLabels = settings_.showEndLabels;
  view.abcdFollowTimer = settings_.abcdFollowTimer;
  view.abcdColour = settings_.abcdColour;
  view.beepMs = sound_.beepMs();
  view.gapMs = sound_.gapMs();
  view.soundEnabled = sound_.isEnabled();
  view.volume = amplifier_.volume();
  view.panelText = panelText_;
  view.traceLevel = tracer_.level();
  view.traceSeq = tracer_.sequence();
  view.freeHeap = ESP.getFreeHeap();
  view.minFreeHeap = ESP.getMinFreeHeap();
  view.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  view.apClients = WiFi.softAPgetStationNum();

  const uint16_t length = Core::renderStateJson(view, json_, sizeof(json_));
  server_.setContentLength(length);
  server_.send(200, "application/json", "");
  server_.sendContent(json_, length);
}

void WebUi::handleControl() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  const String body = server_.arg("plain");
  const String action = readJsonString(body, "action");
  const uint32_t now = now_;
  traceInput(action.c_str());

  if (action == "start") {
    clock_.start(now);
  } else if (action == "stop") {
    clock_.stop(now);
  } else if (action == "line_clear") {
    clock_.lineClear(now);
  } else if (action == "next_end") {
    clock_.nextEnd(now);
  } else if (action == "reset_end") {
    clock_.resetEnd(now);
  } else if (action == "suspend") {
    clock_.suspend(now);
  } else if (action == "resume") {
    clock_.resume(now);
  } else if (action == "add_arrow") {
    clock_.addArrow(now);
  } else if (action == "remove_arrow") {
    clock_.removeArrow(now);
  } else if (action == "emergency") {
    clock_.emergency(now);
  } else if (action == "clear_emergency") {
    clock_.clearEmergency(now);
  } else if (action == "extend") {
    const int seconds = readJsonInteger(body, "seconds");
    if (seconds > 0) clock_.extendTime(now, static_cast<uint32_t>(seconds) * 1000UL);
  } else {
    server_.send(400, "application/json", "{\"error\":\"unknown action\"}");
    return;
  }

  handleState();
}

void WebUi::handleSession() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  traceInput("session");
  const String body = server_.arg("plain");
  Core::SessionConfig config = clock_.config();

  uint8_t index = 0;
  const String mode = readJsonString(body, "mode");
  if (mode.length() && lookup(MODE_NAMES, mode, index)) {
    const Core::Mode requested = static_cast<Core::Mode>(index);
    if (!modeImplemented(requested)) {
      // Refuse rather than run a mode the engine does not implement yet; a
      // director must never be left believing the clock is following a rule
      // that is not written.
      tracer_.warn(now_, "mode_unavailable", Core::name(requested));
      server_.send(409, "application/json", "{\"error\":\"mode not implemented\"}");
      return;
    }
    config.mode = requested;
  }

  const String eventClass = readJsonString(body, "eventClass");
  if (eventClass.length() && lookup(EVENT_CLASS_NAMES, eventClass, index)) {
    config.eventClass = static_cast<Rules::EventClass>(index);
  }

  const int arrows = readJsonInteger(body, "arrowsPerEnd");
  if (arrows == Rules::ARROWS_PER_END_SHORT || arrows == Rules::ARROWS_PER_END_LONG) {
    config.arrowsPerEnd = static_cast<uint8_t>(arrows);
  }

  const int firstShooter = readJsonInteger(body, "firstShooter");
  if (firstShooter == 1 || firstShooter == 2) config.firstShooter = static_cast<uint8_t>(firstShooter);

  const int details = readJsonInteger(body, "details");
  if (details >= 1 && details <= 4) config.details = static_cast<uint8_t>(details);

  const int practiceSeconds = readJsonInteger(body, "practiceSeconds");
  if (practiceSeconds > 0) config.practiceMs = static_cast<uint32_t>(practiceSeconds) * 1000UL;

  uint8_t divisionIndex = settings_.division;
  const String division = readJsonString(body, "division");
  if (division.length() && lookup(DIVISION_NAMES, division, divisionIndex)) settings_.division = divisionIndex;

  const bool matchLogic = readJsonBool(body, "matchLogic", settings_.matchLogic);
  settings_.matchLogic = matchLogic;

  config.replayOccupyOnResume = readJsonBool(body, "resumeOccupy", config.replayOccupyOnResume);
  config.signalEachAlternatingPeriod = readJsonBool(body, "signalEachPeriod", config.signalEachAlternatingPeriod);
  config.abcdRotation = readJsonBool(body, "abcdRotation", config.abcdRotation);
  config.shootOff = readJsonBool(body, "shootOff", config.shootOff);
  config.breakEnabled = readJsonBool(body, "breakEnabled", config.breakEnabled);
  if (body.indexOf("breakAfterEnds") >= 0) {
    const int breakAfterEnds = readJsonInteger(body, "breakAfterEnds");
    if (breakAfterEnds >= 0 && breakAfterEnds <= 36) {
      config.breakAfterEnds = static_cast<uint8_t>(breakAfterEnds);
    }
  }
  if (body.indexOf("breakSeconds") >= 0) {
    const int breakSeconds = readJsonInteger(body, "breakSeconds");
    if (breakSeconds > 0) config.breakMs = static_cast<uint32_t>(breakSeconds) * 1000UL;
  }

  clock_.configure(now_, config);
  applySessionConfig(settings_, config);
  settings_.arrowsPerEnd = clock_.snapshot().arrowsPerEnd;
  match_.configure(now_, matchConfigFrom(settings_));
  match_.enable(now_, settings_.matchLogic);
  store_.save(settings_);
  handleState();
}

void WebUi::handleDisplay() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  traceInput("display");
  uint8_t index = 0;
  const String content = readJsonString(server_.arg("plain"), "content");
  if (content.length() && lookup(CONTENT_NAMES, content, index)) {
    const Core::DisplayContent requested = static_cast<Core::DisplayContent>(index);
    if (!DisplayLogic::contentAvailable(requested)) {
      tracer_.warn(now_, "content_unavailable", Core::name(requested));
      server_.send(409, "application/json", "{\"error\":\"content not available\"}");
      return;
    }
    clock_.setDisplayContent(now_, requested);
    settings_.displayContent = index;
  }
  if (server_.arg("plain").indexOf("clockSeconds") >= 0) {
    const bool clockSeconds = readJsonBool(server_.arg("plain"), "clockSeconds", settings_.clockSeconds);
    tracer_.config(now_, "clock_seconds", settings_.clockSeconds ? 1 : 0, clockSeconds ? 1 : 0);
    settings_.clockSeconds = clockSeconds;
  }
  if (server_.arg("plain").indexOf("showAbcd") >= 0) {
    settings_.showAbcd = readJsonBool(server_.arg("plain"), "showAbcd", settings_.showAbcd);
  }
  if (server_.arg("plain").indexOf("abcdVertical") >= 0) {
    settings_.abcdVertical = readJsonBool(server_.arg("plain"), "abcdVertical", settings_.abcdVertical);
  }
  if (server_.arg("plain").indexOf("showEndLabels") >= 0) {
    settings_.showEndLabels = readJsonBool(server_.arg("plain"), "showEndLabels", settings_.showEndLabels);
  }
  if (server_.arg("plain").indexOf("abcdFollowTimer") >= 0) {
    settings_.abcdFollowTimer =
        readJsonBool(server_.arg("plain"), "abcdFollowTimer", settings_.abcdFollowTimer);
  }
  if (server_.arg("plain").indexOf("abcdColour") >= 0) {
    const String colour = readJsonString(server_.arg("plain"), "abcdColour");
    if (colour.length()) {
      settings_.abcdColour = DisplayLogic::parseCssColour(colour.c_str(), settings_.abcdColour);
    }
  }
  store_.save(settings_);
  handleState();
}

void WebUi::handleBrightness() {
  traceInput("brightness");
  if (server_.hasArg("plain")) {
    const int brightness = readJsonInteger(server_.arg("plain"), "brightness");
    if (brightness >= 1 && brightness <= 255) {
      tracer_.config(now_, "brightness", settings_.brightness, brightness);
      settings_.brightness = static_cast<uint8_t>(brightness);
      store_.save(settings_);
    }
  }
  handleState();
}

void WebUi::handleSound() {
  traceInput("sound");
  if (server_.hasArg("plain")) {
    const String body = server_.arg("plain");
    const int beep = readJsonInteger(body, "beepMs");
    const int gap = readJsonInteger(body, "gapMs");
    if (beep > 0 && gap > 0) {
      tracer_.config(now_, "beep_ms", sound_.beepMs(), beep);
      tracer_.config(now_, "gap_ms", sound_.gapMs(), gap);
      sound_.setPattern(static_cast<uint16_t>(beep), static_cast<uint16_t>(gap));
      settings_.beepMs = static_cast<uint16_t>(beep);
      settings_.gapMs = static_cast<uint16_t>(gap);
    }
    if (body.indexOf("volume") >= 0) {
      const uint8_t volume = Core::clampVolume(readJsonInteger(body, "volume"));
      tracer_.config(now_, "volume", settings_.volume, volume);
      settings_.volume = volume;
      amplifier_.setVolume(volume);
    }
    store_.save(settings_);
    sound_.setEnabled(readJsonBool(body, "enabled", sound_.isEnabled()));
  }
  handleState();
}

void WebUi::handleSoundTest() {
  traceInput("sound_test");
  amplifier_.playTestTone(now_, 2000);
  handleState();
}

void WebUi::handleScore() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  const String body = server_.arg("plain");
  const String action = readJsonString(body, "action");
  const int side = readJsonInteger(body, "side");
  const uint32_t now = now_;
  traceInput(("score_" + action).c_str());

  if (!match_.isEnabled()) {
    server_.send(409, "application/json", "{\"error\":\"match logic is off\"}");
    return;
  }
  if (side < 0 || side > 1) {
    if (action != "complete_end" && action != "reset") {
      server_.send(400, "application/json", "{\"error\":\"side must be 0 or 1\"}");
      return;
    }
  }

  bool accepted = true;
  if (action == "arrow") {
    // "X" arrives as a name because it is not a number; everything else is.
    const String value = readJsonString(body, "value");
    accepted = match_.recordArrow(now, static_cast<uint8_t>(side),
                                  value == "X" ? Core::ARROW_X
                                               : static_cast<uint8_t>(readJsonInteger(body, "value")));
  } else if (action == "undo") {
    accepted = match_.removeLastArrow(now, static_cast<uint8_t>(side));
  } else if (action == "forfeit") {
    // Art. 13.3 for an arrow out of time or sequence, Art. 13.6.2 for a yellow
    // card the team did not act on. The judge says which.
    const String article = readJsonString(body, "article");
    accepted = match_.forfeitHighest(now, static_cast<uint8_t>(side), article == "13.6.2" ? "13.6.2" : "13.3");
  } else if (action == "yellow") {
    match_.yellowCard(now, static_cast<uint8_t>(side));
  } else if (action == "clear_yellow") {
    match_.clearYellowCard(now, static_cast<uint8_t>(side));
  } else if (action == "warn") {
    match_.warn(now, static_cast<uint8_t>(side));
  } else if (action == "disqualify") {
    match_.disqualify(now, static_cast<uint8_t>(side));
  } else if (action == "complete_end") {
    match_.completeEnd(now);
  } else if (action == "reset") {
    match_.reset(now);
  } else {
    server_.send(400, "application/json", "{\"error\":\"unknown action\"}");
    return;
  }

  if (!accepted) {
    server_.send(409, "application/json", "{\"error\":\"refused, see the log\"}");
    return;
  }
  handleState();
}

void WebUi::handleLog() {
  // Streamed, and only the records the caller has not seen. Assembling the
  // whole buffer into one String was an eight-kilobyte allocation every time
  // the console polled, which is precisely the kind of request that fails once
  // the heap is fragmented - and fragmenting it was the same allocation.
  const uint32_t after = server_.hasArg("after") ? server_.arg("after").toInt() : 0;

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");
  server_.sendContent("{\"lines\":[");

  bool first = true;
  for (uint8_t index = 0; index < log_.count(); index++) {
    if (log_.sequence(index) <= after) continue;
    if (!first) server_.sendContent(",");
    first = false;
    server_.sendContent(log_.line(index));
  }

  server_.sendContent("]}");
  server_.sendContent("");
}

void WebUi::handleTrace() {
  traceInput("trace_level");
  if (server_.hasArg("plain")) {
    Core::TraceLevel level = tracer_.level();
    if (Core::parseTraceLevel(readJsonString(server_.arg("plain"), "level").c_str(), level)) {
      tracer_.configText(now_, "trace_level", Core::name(tracer_.level()), Core::name(level));
      tracer_.setLevel(level);
      // Emitted after the change so switching on always leaves a marker in the
      // log saying when recording started.
      tracer_.configText(now_, "trace_level", "", Core::name(level));
      settings_.traceLevel = static_cast<uint8_t>(level);
      store_.save(settings_);
    }
  }
  handleState();
}

int WebUi::readJsonInteger(const String& body, const char* key) const {
  const int keyPosition = body.indexOf(key);
  if (keyPosition < 0) return 0;
  const int separator = body.indexOf(':', keyPosition);
  return separator < 0 ? 0 : body.substring(separator + 1).toInt();
}

String WebUi::readJsonString(const String& body, const char* key) const {
  const int keyPosition = body.indexOf(key);
  if (keyPosition < 0) return String();
  const int separator = body.indexOf(':', keyPosition);
  const int start = separator < 0 ? -1 : body.indexOf('"', separator + 1);
  const int end = start < 0 ? -1 : body.indexOf('"', start + 1);
  return start < 0 || end < 0 ? String() : body.substring(start + 1, end);
}

bool WebUi::readJsonBool(const String& body, const char* key, bool fallback) const {
  const int keyPosition = body.indexOf(key);
  if (keyPosition < 0) return fallback;
  const int separator = body.indexOf(':', keyPosition);
  if (separator < 0) return fallback;
  const String rest = body.substring(separator + 1, separator + 8);
  if (rest.indexOf("true") >= 0) return true;
  if (rest.indexOf("false") >= 0) return false;
  return fallback;
}

