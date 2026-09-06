#include "state_json.h"

#include "display_logic.h"
#include "json_writer.h"

namespace Core {

uint16_t renderStateJson(const StateView& view, char* buffer, uint16_t size) {
  JsonWriter json(buffer, size);
  json.beginObject();

  if (view.snapshot != nullptr) {
    const StateSnapshot& state = *view.snapshot;
    json.unsigned32("schema", state.schemaVersion);
    json.text("phase", name(state.phase));
    json.text("light", name(state.light));
    json.text("mode", name(state.mode));
    json.text("display", name(state.display));
    json.unsigned32("remainingMs", state.remainingMs);
    json.unsigned32("periodMs", state.periodMs);
    json.unsigned32("end", state.endNumber);
    json.unsigned32("arrowsShot", state.arrowsShot);
    json.unsigned32("arrowsPerEnd", state.arrowsPerEnd);
    json.unsigned32("shooter", state.shooter);
    json.pair("sideArrows", state.sideArrows[0], state.sideArrows[1]);
    json.pair("sideRemainingMs", state.sideRemainingMs[0], state.sideRemainingMs[1]);
    json.unsigned32("detail", state.detail);
    json.unsigned32("details", state.details);
    json.boolean("shootOff", state.shootOff);
    json.boolean("running", state.running);
    json.boolean("finished", state.finished);
  }

  if (view.session != nullptr) {
    const SessionConfig& session = *view.session;
    json.text("eventClass", Rules::name(session.eventClass));
    json.boolean("resumeOccupy", session.replayOccupyOnResume);
    json.unsigned32("firstShooter", session.firstShooter);
    json.boolean("signalEachPeriod", session.signalEachAlternatingPeriod);
    json.boolean("abcdRotation", session.abcdRotation);
    json.unsigned32("practiceSeconds", session.practiceMs / 1000);
    json.boolean("breakEnabled", session.breakEnabled);
    json.unsigned32("breakAfterEnds", session.breakAfterEnds);
    json.unsigned32("breakSeconds", session.breakMs / 1000);
  }

  json.unsigned32("perArrowMs", view.perArrowMs);
  json.unsigned32("brightness", view.brightness);
  json.boolean("clockSeconds", view.clockSeconds);
  json.boolean("showAbcd", view.showAbcd);
  json.boolean("abcdVertical", view.abcdVertical);
  json.boolean("showEndLabels", view.showEndLabels);
  json.boolean("abcdFollowTimer", view.abcdFollowTimer);
  char abcdColour[8] = {};
  DisplayLogic::formatCssColour(view.abcdColour, abcdColour, sizeof(abcdColour));
  json.text("abcdColour", abcdColour);
  json.unsigned32("beepMs", view.beepMs);
  json.unsigned32("gapMs", view.gapMs);
  json.boolean("soundEnabled", view.soundEnabled);
  json.unsigned32("volume", view.volume);
  json.text("panelText", view.panelText);
  json.text("traceLevel", name(view.traceLevel));
  json.unsigned32("traceSeq", view.traceSeq);
  json.unsigned32("freeHeap", view.freeHeap);
  json.unsigned32("minFreeHeap", view.minFreeHeap);
  json.unsigned32("largestFreeBlock", view.largestFreeBlock);
  json.unsigned32("apClients", view.apClients);

  json.boolean("matchEnabled", view.matchEnabled);
  if (view.matchConfig != nullptr) {
    json.text("division", name(view.matchConfig->division));
    json.text("scoring", name(view.matchConfig->scoring));
  }
  if (view.match != nullptr) {
    const MatchState& match = *view.match;
    json.text("outcome", name(match.outcome));
    json.unsigned32("matchEnd", match.endNumber);
    json.pair("endTotal", match.endTotal[0], match.endTotal[1]);
    json.pair("runningTotal", match.runningTotal[0], match.runningTotal[1]);
    json.pair("setPoints", match.setPoints[0], match.setPoints[1]);
    json.pair("warnings", match.warnings[0], match.warnings[1]);
    json.beginArray("yellowCard");
    json.arrayBoolean(match.yellowCard[0]);
    json.arrayBoolean(match.yellowCard[1]);
    json.endArray();
    json.beginArray("disqualified");
    json.arrayBoolean(match.disqualified[0]);
    json.arrayBoolean(match.disqualified[1]);
    json.endArray();

    for (uint8_t side = 0; side < 2; side++) {
      json.beginArray(side == 0 ? "arrowsA" : "arrowsB");
      for (uint8_t index = 0; index < match.arrowCount[side]; index++) {
        json.beginObject();
        json.unsigned32("v", match.arrows[side][index]);
        json.boolean("lost", match.forfeited[side][index]);
        json.endObject();
      }
      json.endArray();
    }
  }

  json.endObject();
  return json.finish();
}

}  // namespace Core
