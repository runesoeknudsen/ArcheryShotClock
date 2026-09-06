#pragma once

#include <cstdint>

#include "match_logic.h"
#include "rules.h"
#include "shot_clock.h"
#include "snapshot.h"
#include "trace.h"

// Renders the whole device state as JSON into a fixed buffer.
//
// The console polls this several times a second. Built with String
// concatenation it was sixty-odd reallocations per response, which on a board
// sharing its heap with a Wi-Fi access point fragments memory until the access
// point itself becomes unreliable. One buffer, written once, cannot.

namespace Core {

// Comfortably larger than the longest response, including six arrows a side.
constexpr uint16_t STATE_JSON_BYTES = 1536;

struct StateView {
  const StateSnapshot* snapshot = nullptr;
  const SessionConfig* session = nullptr;
  const MatchState* match = nullptr;
  const MatchConfig* matchConfig = nullptr;
  bool matchEnabled = false;

  uint32_t perArrowMs = 0;
  uint8_t brightness = 0;
  bool clockSeconds = false;
  bool showAbcd = true;
  bool abcdVertical = true;
  bool showEndLabels = true;
  bool abcdFollowTimer = false;
  uint32_t abcdColour = 0xFFFFFF;
  uint16_t beepMs = 0;
  uint16_t gapMs = 0;
  bool soundEnabled = true;
  uint8_t volume = 0;
  const char* panelText = "";

  TraceLevel traceLevel = TraceLevel::Off;
  uint32_t traceSeq = 0;

  // Reported so the console can show the board's memory rather than leaving a
  // failing access point looking like a network problem.
  uint32_t freeHeap = 0;
  uint32_t minFreeHeap = 0;
  uint32_t largestFreeBlock = 0;
  uint8_t apClients = 0;
};

// Returns the number of bytes written. Never exceeds size.
uint16_t renderStateJson(const StateView& view, char* buffer, uint16_t size);

}  // namespace Core
