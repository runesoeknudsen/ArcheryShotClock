#include "signals.h"

namespace Core {

const char* name(SignalCode code) {
  switch (code) {
    case SignalCode::None: return "NONE";
    case SignalCode::OccupyLine: return "OCCUPY_LINE";
    case SignalCode::Start: return "START";
    case SignalCode::Stop: return "STOP";
    case SignalCode::Scoring: return "SCORING";
    case SignalCode::Resume: return "RESUME";
    case SignalCode::Emergency: return "EMERGENCY";
  }
  return "UNKNOWN";
}

uint8_t signalCount(SignalCode code) {
  switch (code) {
    case SignalCode::OccupyLine: return Rules::SIGNALS_OCCUPY_LINE;
    case SignalCode::Start: return Rules::SIGNALS_START;
    case SignalCode::Stop: return Rules::SIGNALS_STOP;
    case SignalCode::Scoring: return Rules::SIGNALS_SCORING;
    case SignalCode::Resume: return Rules::SIGNALS_RESUME;
    case SignalCode::Emergency: return Rules::SIGNALS_EMERGENCY_MINIMUM;
    case SignalCode::None: return 0;
  }
  return 0;
}

}  // namespace Core
