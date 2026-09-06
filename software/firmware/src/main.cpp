#include <WiFi.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "core/match_logic.h"
#include "core/shot_clock.h"
#include "core/snapshot.h"
#include "core/sound.h"
#include "core/trace.h"
#include "display.h"
#include "hal/buttons.h"
#include "hal/log_buffer.h"
#include "hal/serial_trace.h"
#include "settings.h"
#include "sound_outputs.h"
#include "web_ui.h"

Settings settings;
SettingsStore settingsStore;
Display display;

SerialTraceSink serialTrace;
LogBufferSink logBuffer;
// The UART and the web UI must show the same log, not two separately generated
// ones, or the two accounts could disagree about what happened.
TeeSink traceSink(serialTrace, logBuffer);
Core::Tracer tracer(traceSink);

Core::ShotClock shotClock(tracer);
Core::MatchLogic match(tracer);
ActiveBuzzerOutput buzzer;
Max98357Output amplifier;
CombinedSoundOutput soundOutput(buzzer, amplifier);
Core::SoundController sound(soundOutput);
WebUi webUi(settings, settingsStore, shotClock, match, sound, amplifier, tracer, logBuffer);
ButtonPanel buttons;
EmergencyStop emergencyStop;

namespace {
Core::StateSnapshot lastSnapshot;
uint32_t lastHeartbeat = 0;
uint32_t lastFrameChecksum = 0;
bool haveFrame = false;

const char* changeCause(const Core::StateSnapshot& previous, const Core::StateSnapshot& current) {
  if (previous.phase != current.phase) return Core::name(current.phase);
  if (previous.arrowsShot != current.arrowsShot) return "arrows";
  if (previous.remainingMs != current.remainingMs) return "tick";
  return "change";
}

void drawPanel(uint32_t now, const Core::StateSnapshot& state) {
  DisplayLogic::RenderRequest style;
  style.clockSeconds = settings.clockSeconds;
  style.showAbcd = settings.showAbcd;
  style.abcdVertical = settings.abcdVertical;
  style.showEndLabels = settings.showEndLabels;
  style.abcdFollowTimer = settings.abcdFollowTimer;
  style.abcdColour = settings.abcdColour;
  const DisplayLogic::RenderResult result = display.render(state, settings.brightness, style);
  webUi.setPanelText(result.text);
  if (haveFrame && result.checksum == lastFrameChecksum) return;
  lastFrameChecksum = result.checksum;
  haveFrame = true;
  tracer.render(now, Core::name(state.display), Core::name(state.light), result.text, result.checksum,
                result.litPixels);
}

// The suspend button toggles, because suspending and resuming are the same
// decision seen from either side and a second button for it would be one more
// thing to reach for under pressure.
void applyButton(uint32_t now, ButtonControl control, bool longPress) {
  tracer.input(now, "btn", name(control), longPress ? 1 : 0);
  switch (control) {
    case ButtonControl::Start: shotClock.start(now); break;
    case ButtonControl::Stop: shotClock.stop(now); break;
    case ButtonControl::LineClear: shotClock.lineClear(now); break;
    case ButtonControl::NextEnd:
      // Closing the end is one decision: the clock moves on and, if the match
      // module is running, the end is scored.
      if (match.isEnabled()) match.completeEnd(now);
      shotClock.nextEnd(now);
      break;
    case ButtonControl::Suspend:
      if (shotClock.snapshot().phase == Core::Phase::Suspended) {
        shotClock.resume(now);
      } else {
        shotClock.suspend(now);
      }
      break;
    case ButtonControl::Count: break;
  }
}

void publishState(uint32_t now) {
  const Core::StateSnapshot& snapshot = shotClock.snapshot();
  if (!snapshot.sameAs(lastSnapshot)) {
    tracer.state(now, snapshot, changeCause(lastSnapshot, snapshot));
    lastSnapshot = snapshot;
    lastHeartbeat = now;
    drawPanel(now, snapshot);
    return;
  }

  if (now - lastHeartbeat >= Config::TRACE_HEARTBEAT_MS) {
    tracer.heartbeat(now, snapshot);
    // Fragmentation is what takes the access point down, and it is invisible
    // without a number, so the heap travels with the heartbeat.
    tracer.health(now, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), WiFi.softAPgetStationNum());
    lastHeartbeat = now;
  }
}
}  // namespace

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  tracer.boot(millis(), Config::FIRMWARE_ID);

  settings = settingsStore.load();
  tracer.setLevel(static_cast<Core::TraceLevel>(settings.traceLevel));
  sound.setPattern(settings.beepMs, settings.gapMs);
  shotClock.configure(millis(), sessionConfigFrom(settings));
  match.configure(millis(), matchConfigFrom(settings));
  match.enable(millis(), settings.matchLogic);
  shotClock.setDisplayContent(millis(), static_cast<Core::DisplayContent>(settings.displayContent));
  tracer.config(millis(), "brightness", 0, settings.brightness);
  tracer.config(millis(), "volume", 0, settings.volume);
  tracer.config(millis(), "beep_ms", 0, settings.beepMs);
  tracer.config(millis(), "gap_ms", 0, settings.gapMs);

  buzzer.begin();
  amplifier.setVolume(settings.volume);
  amplifier.begin();
  display.begin();
  buttons.begin();
  // Armed before Wi-Fi comes up, so the emergency stop works from the first
  // moment the board is running rather than from the moment the AP is ready.
  emergencyStop.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(Config::AP_NAME, Config::AP_PASSWORD);
  tracer.configText(millis(), "ap_ip", "", WiFi.softAPIP().toString().c_str());
  webUi.begin();

  lastSnapshot = shotClock.snapshot();
  tracer.state(millis(), lastSnapshot, "boot");
  drawPanel(millis(), lastSnapshot);
  lastHeartbeat = millis();
}

void loop() {
  const uint32_t now = millis();

  // The emergency stop is served before anything else in the loop, and its
  // timestamp comes from the interrupt rather than from here.
  uint32_t emergencyAt = 0;
  if (emergencyStop.taken(emergencyAt)) {
    tracer.input(emergencyAt, "btn", "emergency", 0);
    shotClock.emergency(emergencyAt);
  }

  ButtonControl control = ButtonControl::Start;
  bool longPress = false;
  while (buttons.poll(now, control, longPress)) applyButton(now, control, longPress);

  webUi.handleClient(now);
  shotClock.update(now);

  // The clock queues signals rather than playing them, so the sound layer and
  // the trace consume the same sequence in the same order.
  Core::SignalCode signal = Core::SignalCode::None;
  while (shotClock.takeSignal(signal)) sound.playSignal(signal, now);

  sound.update(now);
  amplifier.update(now);

  shotClock.setMatchTotals(match.state().runningTotal, match.state().setPoints);
  publishState(now);

  const uint32_t droppedRecords = serialTrace.takeDropped();
  if (droppedRecords > 0) tracer.dropped(now, droppedRecords);
  serialTrace.drain();
}
