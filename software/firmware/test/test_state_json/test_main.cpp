#include <unity.h>

#include <cstring>
#include <string>

#include "core/json_writer.h"
#include "core/state_json.h"

namespace {
char buffer[Core::STATE_JSON_BYTES];

std::string render(const Core::StateView& view) {
  const uint16_t length = Core::renderStateJson(view, buffer, sizeof(buffer));
  return std::string(buffer, length);
}

bool has(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }

// Counts braces and brackets to prove the document closes cleanly.
bool balanced(const std::string& text) {
  int braces = 0;
  int brackets = 0;
  bool inString = false;
  for (size_t index = 0; index < text.size(); index++) {
    const char character = text[index];
    if (inString) {
      if (character == '\\') index++;
      else if (character == '"') inString = false;
      continue;
    }
    if (character == '"') inString = true;
    else if (character == '{') braces++;
    else if (character == '}') braces--;
    else if (character == '[') brackets++;
    else if (character == ']') brackets--;
    if (braces < 0 || brackets < 0) return false;
  }
  return braces == 0 && brackets == 0 && !inString;
}
}  // namespace

void test_writer_produces_a_closed_document_with_no_heap() {
  char small[64];
  Core::JsonWriter json(small, sizeof(small));
  json.beginObject();
  json.text("a", "one");
  json.unsigned32("b", 2);
  json.boolean("c", true);
  json.pair("d", 4, 5);
  json.endObject();
  json.finish();

  TEST_ASSERT_EQUAL_STRING("{\"a\":\"one\",\"b\":2,\"c\":true,\"d\":[4,5]}", small);
  TEST_ASSERT_FALSE(json.overflowed());
}

void test_writer_closes_the_document_even_when_the_buffer_runs_out() {
  char tiny[32];
  Core::JsonWriter json(tiny, sizeof(tiny));
  json.beginObject();
  json.beginArray("values");
  for (uint8_t index = 0; index < 40; index++) json.arrayNumber(123456);
  json.endArray();
  json.endObject();
  json.finish();

  // Truncated, but still parseable rather than stopping mid-number.
  TEST_ASSERT_TRUE(json.overflowed());
  TEST_ASSERT_TRUE(balanced(std::string(tiny)));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(sizeof(tiny), strlen(tiny) + 1);
}

void test_writer_escapes_strings() {
  char small[96];
  Core::JsonWriter json(small, sizeof(small));
  json.beginObject();
  json.text("quote", "say \"hi\"\nnow");
  json.endObject();
  json.finish();

  TEST_ASSERT_EQUAL_STRING("{\"quote\":\"say \\\"hi\\\"\\nnow\"}", small);
}

void test_state_json_carries_the_clock_and_the_session() {
  Core::StateSnapshot snapshot;
  snapshot.phase = Core::Phase::Warning;
  snapshot.light = Core::Light::Yellow;
  snapshot.mode = Core::Mode::IndividualNonAlternating;
  snapshot.remainingMs = 30000;
  snapshot.periodMs = 90000;
  snapshot.endNumber = 3;
  snapshot.arrowsShot = 2;
  snapshot.arrowsPerEnd = 3;

  Core::SessionConfig session;
  session.eventClass = Rules::EventClass::Announced;

  Core::StateView view;
  view.snapshot = &snapshot;
  view.session = &session;
  view.perArrowMs = 30000;
  view.panelText = "00:30";
  view.volume = 60;

  const std::string json = render(view);

  TEST_ASSERT_TRUE(balanced(json));
  TEST_ASSERT_TRUE(has(json, "\"phase\":\"WARNING\""));
  TEST_ASSERT_TRUE(has(json, "\"light\":\"YELLOW\""));
  TEST_ASSERT_TRUE(has(json, "\"remainingMs\":30000"));
  TEST_ASSERT_TRUE(has(json, "\"eventClass\":\"ANNOUNCED\""));
  TEST_ASSERT_TRUE(has(json, "\"perArrowMs\":30000"));
  TEST_ASSERT_TRUE(has(json, "\"panelText\":\"00:30\""));
  TEST_ASSERT_TRUE(has(json, "\"volume\":60"));
  TEST_ASSERT_TRUE(has(json, "\"clockSeconds\":false"));
  TEST_ASSERT_TRUE(has(json, "\"showAbcd\":true"));
  TEST_ASSERT_TRUE(has(json, "\"showEndLabels\":true"));
  TEST_ASSERT_TRUE(has(json, "\"abcdFollowTimer\":false"));
  TEST_ASSERT_TRUE(has(json, "\"abcdColour\":\"#ffffff\""));
  TEST_ASSERT_TRUE(has(json, "\"abcdRotation\":true"));
}

void test_state_json_reports_the_board_s_memory() {
  Core::StateSnapshot snapshot;
  Core::StateView view;
  view.snapshot = &snapshot;
  view.freeHeap = 120000;
  view.minFreeHeap = 98000;
  view.largestFreeBlock = 60000;
  view.apClients = 2;
  view.traceLevel = Core::TraceLevel::Minimal;

  const std::string json = render(view);

  // A failing access point looks like a network fault until the heap is
  // visible, so these travel with every state response.
  TEST_ASSERT_TRUE(has(json, "\"freeHeap\":120000"));
  TEST_ASSERT_TRUE(has(json, "\"minFreeHeap\":98000"));
  TEST_ASSERT_TRUE(has(json, "\"largestFreeBlock\":60000"));
  TEST_ASSERT_TRUE(has(json, "\"apClients\":2"));
  TEST_ASSERT_TRUE(has(json, "\"traceLevel\":\"MINIMAL\""));
}

void test_state_json_includes_the_match_when_one_is_running() {
  Core::StateSnapshot snapshot;
  Core::MatchConfig config;
  Core::MatchState match;
  match.endNumber = 2;
  match.arrowCount[0] = 2;
  match.arrows[0][0] = Core::ARROW_X;
  match.arrows[0][1] = 9;
  match.forfeited[0][0] = true;
  match.setPoints[0] = 4;
  match.runningTotal[0] = 57;
  match.yellowCard[1] = true;

  Core::StateView view;
  view.snapshot = &snapshot;
  view.match = &match;
  view.matchConfig = &config;
  view.matchEnabled = true;

  const std::string json = render(view);

  TEST_ASSERT_TRUE(balanced(json));
  TEST_ASSERT_TRUE(has(json, "\"matchEnabled\":true"));
  TEST_ASSERT_TRUE(has(json, "\"division\":\"RECURVE\""));
  TEST_ASSERT_TRUE(has(json, "\"setPoints\":[4,0]"));
  TEST_ASSERT_TRUE(has(json, "\"runningTotal\":[57,0]"));
  TEST_ASSERT_TRUE(has(json, "\"yellowCard\":[false,true]"));
  TEST_ASSERT_TRUE(has(json, "\"arrowsA\":[{\"v\":11,\"lost\":true},{\"v\":9,\"lost\":false}]"));
  TEST_ASSERT_TRUE(has(json, "\"arrowsB\":[]"));
}

void test_the_longest_response_still_fits_the_buffer() {
  Core::StateSnapshot snapshot;
  snapshot.display = Core::DisplayContent::SetPoints;
  Core::SessionConfig session;
  Core::MatchConfig config;
  config.team = true;
  config.arrowsPerEnd = 6;

  Core::MatchState match;
  match.endNumber = 4;
  for (uint8_t side = 0; side < 2; side++) {
    match.arrowCount[side] = Core::ARROWS_MAX;
    for (uint8_t index = 0; index < Core::ARROWS_MAX; index++) {
      match.arrows[side][index] = Core::ARROW_X;
      match.forfeited[side][index] = index == 0;
    }
    match.runningTotal[side] = 60000;
  }

  Core::StateView view;
  view.snapshot = &snapshot;
  view.session = &session;
  view.match = &match;
  view.matchConfig = &config;
  view.matchEnabled = true;
  view.panelText = "60-60";

  const uint16_t length = Core::renderStateJson(view, buffer, sizeof(buffer));

  TEST_ASSERT_LESS_THAN_UINT16(Core::STATE_JSON_BYTES, length);
  TEST_ASSERT_TRUE(balanced(std::string(buffer, length)));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_writer_produces_a_closed_document_with_no_heap);
  RUN_TEST(test_writer_closes_the_document_even_when_the_buffer_runs_out);
  RUN_TEST(test_writer_escapes_strings);
  RUN_TEST(test_state_json_carries_the_clock_and_the_session);
  RUN_TEST(test_state_json_reports_the_board_s_memory);
  RUN_TEST(test_state_json_includes_the_match_when_one_is_running);
  RUN_TEST(test_the_longest_response_still_fits_the_buffer);
  return UNITY_END();
}
