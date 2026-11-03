#pragma once

#include <WebServer.h>

#include "core/match_logic.h"
#include "core/state_json.h"
#include "core/shot_clock.h"
#include "core/sound.h"
#include "core/trace.h"
#include "hal/log_buffer.h"
#include "speaker_output.h"
#include "settings.h"

// The director's console. Everything the firmware knows is visible here, and
// every control is reachable here, so a tablet is a complete substitute for
// standing at the box - with the exception of the emergency stop, which will
// get a physical button in Phase 2 precisely because it must not depend on
// Wi-Fi being up.
class WebUi {
public:
  WebUi(Settings& settings, SettingsStore& store, Core::ShotClock& clock, Core::MatchLogic& match,
        Core::SoundController& sound, SpeakerOutput& speaker, Core::Tracer& tracer, LogBufferSink& log);

  void begin();
  // Takes the loop's timestamp so every command is stamped with the same
  // instant the engine is about to be updated with. Reading millis() inside a
  // handler produced a time later than the update that followed it, and the
  // engine then saw its clock run backwards.
  void handleClient(uint32_t now);

  // What the panel last drew, mirrored back to the browser so the director can
  // see the display without looking at it.
  void setPanelText(const char* text);

private:
  void sendPage(const uint8_t* page, size_t length);
  void handleState();
  void handleControl();
  void handleSession();
  void handleDisplay();
  void handleBrightness();
  void handleSound();
  void handleScore();
  void handleTrace();
  void handleLog();
  void handleSoundTest();

  void traceInput(const char* control);
  int readJsonInteger(const String& body, const char* key) const;
  String readJsonString(const String& body, const char* key) const;
  bool readJsonBool(const String& body, const char* key, bool fallback) const;

  Settings& settings_;
  SettingsStore& store_;
  Core::ShotClock& clock_;
  Core::MatchLogic& match_;
  Core::SoundController& sound_;
  SpeakerOutput& speaker_;
  Core::Tracer& tracer_;
  LogBufferSink& log_;
  WebServer server_{80};
  char panelText_[64] = {0};
  char panelLines_[128] = {0};
  // One response buffer, reused. Building these with String concatenation was
  // what made the access point unreliable.
  char json_[Core::STATE_JSON_BYTES];
  uint32_t now_ = 0;
};
