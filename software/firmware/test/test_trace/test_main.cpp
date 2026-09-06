#include <unity.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/trace.h"

// Collects emitted lines so the tests can assert on the exact text that would
// go out of the UART.
class CapturingSink : public Core::TraceSink {
public:
  void write(const char* line, uint16_t length) override {
    lines.push_back(std::string(line, length));
    text.append(line, length);
  }

  const std::string& last() const { return lines.back(); }
  bool lastContains(const char* needle) const { return last().find(needle) != std::string::npos; }

  std::vector<std::string> lines;
  std::string text;
};

void test_boot_record_is_self_describing() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);

  tracer.boot(12, "test");

  TEST_ASSERT_EQUAL_UINT32(1, tracer.sequence());
  TEST_ASSERT_TRUE(sink.lastContains("\"r\":\"BOOT\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"t\":12"));
  TEST_ASSERT_TRUE(sink.lastContains("\"seq\":1"));
  TEST_ASSERT_TRUE(sink.lastContains("\"schema\":3"));
  TEST_ASSERT_TRUE(sink.lastContains("\"fw\":\"test\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"level\":\"NORMAL\""));
}

void test_recording_is_off_by_default_but_faults_are_never_hidden() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  Core::StateSnapshot snapshot;

  // Off is the default because recording costs time and memory on the board,
  // and an event is not the moment to spend either.
  TEST_ASSERT_EQUAL(Core::TraceLevel::Off, tracer.level());
  TEST_ASSERT_FALSE(tracer.isRecording());

  tracer.state(1, snapshot, "change");
  tracer.signal(2, "STOP", 2, "11.3.1");
  tracer.rule(3, "11.2.1.1", "period", nullptr, 0, nullptr);
  tracer.render(4, "CLOCK", "RED", "00:00", 1, 2);
  tracer.config(5, "brightness", 1, 2);
  TEST_ASSERT_EQUAL_UINT32(0, tracer.sequence());

  // A fault that could be switched off would be a fault nobody hears about.
  tracer.boot(6, "test");
  tracer.warn(7, "code", "detail");
  tracer.error(8, "code", "detail");
  TEST_ASSERT_EQUAL_UINT32(3, tracer.sequence());
}

void test_trace_levels_round_trip_through_their_names() {
  Core::TraceLevel level = Core::TraceLevel::Verbose;

  TEST_ASSERT_TRUE(Core::parseTraceLevel("OFF", level));
  TEST_ASSERT_EQUAL(Core::TraceLevel::Off, level);
  TEST_ASSERT_TRUE(Core::parseTraceLevel("MINIMAL", level));
  TEST_ASSERT_EQUAL(Core::TraceLevel::Minimal, level);
  TEST_ASSERT_TRUE(Core::parseTraceLevel("NORMAL", level));
  TEST_ASSERT_EQUAL(Core::TraceLevel::Normal, level);
  TEST_ASSERT_FALSE(Core::parseTraceLevel("LOUD", level));
  TEST_ASSERT_EQUAL(Core::TraceLevel::Normal, level);
  TEST_ASSERT_EQUAL_STRING("VERBOSE", Core::name(Core::TraceLevel::Verbose));
}

void test_health_records_carry_the_heap() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);

  tracer.health(900, 120000, 98000, 60000, 2);

  TEST_ASSERT_TRUE(sink.lastContains("\"r\":\"HEALTH\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"heap\":120000"));
  TEST_ASSERT_TRUE(sink.lastContains("\"heap_min\":98000"));
  TEST_ASSERT_TRUE(sink.lastContains("\"ap_clients\":2"));
}

void test_every_line_is_one_terminated_json_object() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);
  Core::StateSnapshot snapshot;

  tracer.boot(0, "test");
  tracer.state(1, snapshot, "boot");
  tracer.signal(2, "START", 1, "11.3.1");

  TEST_ASSERT_EQUAL_UINT32(3, sink.lines.size());
  for (const std::string& line : sink.lines) {
    TEST_ASSERT_EQUAL_CHAR('{', line.front());
    TEST_ASSERT_EQUAL_CHAR('\n', line.back());
    TEST_ASSERT_EQUAL_CHAR('}', line[line.size() - 2]);
    TEST_ASSERT_EQUAL_UINT32(1, std::count(line.begin(), line.end(), '\n'));
  }
}

void test_sequence_increments_by_exactly_one() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);
  Core::StateSnapshot snapshot;

  for (uint32_t index = 0; index < 5; index++) tracer.state(index, snapshot, "tick");

  TEST_ASSERT_EQUAL_UINT32(5, tracer.sequence());
  TEST_ASSERT_TRUE(sink.lines[0].find("\"seq\":1") != std::string::npos);
  TEST_ASSERT_TRUE(sink.lines[4].find("\"seq\":5") != std::string::npos);
}

void test_state_record_carries_the_whole_snapshot() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);
  Core::StateSnapshot snapshot;
  snapshot.phase = Core::Phase::Warning;
  snapshot.light = Core::Light::Yellow;
  snapshot.mode = Core::Mode::TeamSimultaneous;
  snapshot.display = Core::DisplayContent::Score;
  snapshot.remainingMs = 30000;
  snapshot.periodMs = 120000;
  snapshot.endNumber = 3;
  snapshot.arrowsShot = 4;
  snapshot.score[0] = 87;
  snapshot.score[1] = 84;
  snapshot.setPoints[0] = 4;
  snapshot.running = true;

  tracer.state(500, snapshot, "tick");

  TEST_ASSERT_TRUE(sink.lastContains("\"phase\":\"WARNING\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"light\":\"YELLOW\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"mode\":\"TEAM_SIMUL\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"disp\":\"SCORE\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"rem_ms\":30000"));
  TEST_ASSERT_TRUE(sink.lastContains("\"per_ms\":120000"));
  TEST_ASSERT_TRUE(sink.lastContains("\"end\":3"));
  TEST_ASSERT_TRUE(sink.lastContains("\"arrows\":4"));
  TEST_ASSERT_TRUE(sink.lastContains("\"score\":[87,84]"));
  TEST_ASSERT_TRUE(sink.lastContains("\"set_points\":[4,0]"));
  TEST_ASSERT_TRUE(sink.lastContains("\"run\":true"));
  TEST_ASSERT_TRUE(sink.lastContains("\"fin\":false"));
}

void test_rule_record_carries_its_inputs_so_it_can_be_recomputed() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);
  const Core::TraceField fields[] = {{"clock_ms", 38000}, {"unshot", 3}, {"floor_ms", 60000}, {"result_ms", 60000}};

  tracer.rule(184320, "11.2.4.2", "resume_recalc", fields, 4, "clock<floor");

  TEST_ASSERT_TRUE(sink.lastContains("\"art\":\"11.2.4.2\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"what\":\"resume_recalc\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"clock_ms\":38000"));
  TEST_ASSERT_TRUE(sink.lastContains("\"unshot\":3"));
  TEST_ASSERT_TRUE(sink.lastContains("\"reason\":\"clock<floor\""));
}

void test_minimal_level_keeps_evidence_and_drops_chatter() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  Core::StateSnapshot snapshot;
  tracer.setLevel(Core::TraceLevel::Minimal);

  tracer.input(1, "web", "start", 0);
  tracer.heartbeat(2, snapshot);
  TEST_ASSERT_EQUAL_UINT32(0, tracer.sequence());

  tracer.state(3, snapshot, "change");
  tracer.signal(4, "STOP", 2, "11.3.1");
  tracer.error(5, "nvs", "write failed");
  TEST_ASSERT_EQUAL_UINT32(3, tracer.sequence());
}

void test_strings_are_escaped_so_a_reader_never_sees_broken_json() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);

  tracer.error(1, "quote\"and\\slash", "line\nbreak\ttab");

  TEST_ASSERT_TRUE(sink.lastContains("\\\"and\\\\slash"));
  TEST_ASSERT_TRUE(sink.lastContains("line\\nbreak\\ttab"));
  TEST_ASSERT_EQUAL_UINT32(1, std::count(sink.last().begin(), sink.last().end(), '\n'));
}

void test_overlong_records_are_marked_not_silently_cut() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);
  std::string huge(Core::TRACE_LINE_MAX * 2, 'x');

  tracer.error(1, "too_long", huge.c_str());

  TEST_ASSERT_TRUE(tracer.lastLineTruncated());
  TEST_ASSERT_TRUE(sink.lastContains("\"trunc\":1"));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(Core::TRACE_LINE_MAX, sink.last().size());
  TEST_ASSERT_EQUAL_CHAR('}', sink.last()[sink.last().size() - 2]);
}

void test_dropped_records_are_reported_rather_than_hidden() {
  CapturingSink sink;
  Core::Tracer tracer(sink);
  tracer.setLevel(Core::TraceLevel::Normal);

  tracer.dropped(900, 17);

  TEST_ASSERT_TRUE(sink.lastContains("\"r\":\"WARN\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"code\":\"trace_dropped\""));
  TEST_ASSERT_TRUE(sink.lastContains("\"count\":17"));
}

void test_snapshot_equality_detects_every_tracked_field() {
  Core::StateSnapshot left;
  Core::StateSnapshot right;
  TEST_ASSERT_TRUE(left.sameAs(right));

  right.remainingMs = 1;
  TEST_ASSERT_FALSE(left.sameAs(right));

  right = left;
  right.setPoints[1] = 2;
  TEST_ASSERT_FALSE(left.sameAs(right));

  right = left;
  right.display = Core::DisplayContent::Blank;
  TEST_ASSERT_FALSE(left.sameAs(right));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_record_is_self_describing);
  RUN_TEST(test_recording_is_off_by_default_but_faults_are_never_hidden);
  RUN_TEST(test_trace_levels_round_trip_through_their_names);
  RUN_TEST(test_health_records_carry_the_heap);
  RUN_TEST(test_every_line_is_one_terminated_json_object);
  RUN_TEST(test_sequence_increments_by_exactly_one);
  RUN_TEST(test_state_record_carries_the_whole_snapshot);
  RUN_TEST(test_rule_record_carries_its_inputs_so_it_can_be_recomputed);
  RUN_TEST(test_minimal_level_keeps_evidence_and_drops_chatter);
  RUN_TEST(test_strings_are_escaped_so_a_reader_never_sees_broken_json);
  RUN_TEST(test_overlong_records_are_marked_not_silently_cut);
  RUN_TEST(test_dropped_records_are_reported_rather_than_hidden);
  RUN_TEST(test_snapshot_equality_detects_every_tracked_field);
  return UNITY_END();
}
