#include "display_logic.h"

#include "big_digits.h"
#include "small_font.h"

namespace DisplayLogic {
namespace {

constexpr uint8_t DIGIT_WIDTH = 5;
constexpr uint8_t DIGIT_HEIGHT = 11;
constexpr uint8_t DIGIT_GAP = 2;
constexpr uint8_t DIGIT_TOP = (ROWS - DIGIT_HEIGHT) / 2;
constexpr uint8_t GROUP_LEFT = 1;
constexpr uint8_t LETTER_GAP = 1;

const char* groupLetters(uint8_t detail) {
  switch (detail) {
    case 2: return "CD";
    case 3: return "EF";
    case 4: return "GH";
    default: return "AB";
  }
}

bool showGroup(const RenderRequest& request) {
  return request.showAbcd && request.details > 1;
}

bool endLabelPhase(Core::Phase phase) {
  return phase == Core::Phase::Finished || phase == Core::Phase::Scoring;
}

void drawDigitAt(uint8_t value, uint8_t left, uint8_t top, uint32_t colour, uint32_t* pixels) {
  if (value > 9) return;
  for (uint8_t row = 0; row < DIGIT_HEIGHT; row++) {
    if (top + row >= ROWS) continue;
    for (uint8_t column = 0; column < DIGIT_WIDTH; column++) {
      if (BIG_DIGITS[value][row][column] != '#') continue;
      const uint8_t x = left + column;
      if (x >= COLUMNS) continue;
      pixels[ledIndex(x, top + row)] = colour;
    }
  }
}

void drawDigit(uint8_t value, uint8_t left, uint32_t colour, uint32_t* pixels) {
  drawDigitAt(value, left, DIGIT_TOP, colour, pixels);
}

void drawColon(uint32_t colour, uint32_t* pixels) {
  for (uint8_t y = 5; y < 7; y++) {
    pixels[ledIndex(15, y)] = colour;
    pixels[ledIndex(16, y)] = colour;
  }
  for (uint8_t y = 9; y < 11; y++) {
    pixels[ledIndex(15, y)] = colour;
    pixels[ledIndex(16, y)] = colour;
  }
}

void drawSeparator(uint32_t colour, uint32_t* pixels) {
  for (uint8_t y = 7; y < 9; y++) {
    for (uint8_t x = 14; x < 18; x++) pixels[ledIndex(x, y)] = colour;
  }
}

void drawNarrowLetter(char letter, uint8_t left, uint8_t top, uint32_t colour, uint32_t* pixels) {
  if (letter < 'A' || letter > 'H') return;
  const uint8_t index = static_cast<uint8_t>(letter - 'A');
  for (uint8_t row = 0; row < SmallFont::NARROW_HEIGHT; row++) {
    if (top + row >= ROWS) continue;
    for (uint8_t column = 0; column < SmallFont::NARROW_WIDTH; column++) {
      if (SmallFont::NARROW[index][row][column] != '#') continue;
      const uint8_t x = left + column;
      if (x >= COLUMNS) continue;
      pixels[ledIndex(x, top + row)] = colour;
    }
  }
}

void drawWideLetter(char letter, uint8_t left, uint8_t top, uint32_t colour, uint32_t* pixels) {
  for (uint8_t row = 0; row < SmallFont::WIDE_HEIGHT; row++) {
    if (top + row >= ROWS) continue;
    const char* glyph = SmallFont::wideRow(letter, row);
    for (uint8_t column = 0; column < SmallFont::WIDE_WIDTH; column++) {
      if (glyph[column] != '#') continue;
      const uint8_t x = left + column;
      if (x >= COLUMNS) continue;
      pixels[ledIndex(x, top + row)] = colour;
    }
  }
}

void drawWideWord(const char* word, uint8_t top, uint32_t colour, uint32_t* pixels) {
  uint8_t count = 0;
  while (word[count] != '\0') count++;
  if (count == 0) return;
  const uint8_t width =
      static_cast<uint8_t>(count * SmallFont::WIDE_WIDTH + (count - 1) * LETTER_GAP);
  uint8_t left = width >= COLUMNS ? 0 : static_cast<uint8_t>((COLUMNS - width) / 2);
  for (uint8_t index = 0; index < count; index++) {
    drawWideLetter(word[index], left, top, colour, pixels);
    left = static_cast<uint8_t>(left + SmallFont::WIDE_WIDTH + LETTER_GAP);
  }
}

void drawGroupVertical(const char* letters, uint32_t colour, uint32_t* pixels) {
  drawNarrowLetter(letters[0], GROUP_LEFT, 2, colour, pixels);
  drawNarrowLetter(letters[1], GROUP_LEFT, 9, colour, pixels);
}

void drawGroupUnder(const char* letters, uint32_t colour, uint32_t* pixels) {
  const uint8_t width =
      static_cast<uint8_t>(SmallFont::NARROW_WIDTH * 2 + LETTER_GAP);
  const uint8_t left = static_cast<uint8_t>(CLOCK_ONES_LEFT + DIGIT_WIDTH - width);
  drawNarrowLetter(letters[0], left, 11, colour, pixels);
  drawNarrowLetter(letters[1], static_cast<uint8_t>(left + SmallFont::NARROW_WIDTH + LETTER_GAP), 11,
                   colour, pixels);
}

// Two two-digit numbers side by side, side A on the left and side B on the
// right, in the same columns the clock uses.
void drawTwoPairs(uint16_t left, uint16_t right, uint32_t colour, uint32_t* pixels) {
  const uint8_t a = left > 99 ? 99 : static_cast<uint8_t>(left);
  const uint8_t b = right > 99 ? 99 : static_cast<uint8_t>(right);
  drawDigit(a / 10, 2, colour, pixels);
  drawDigit(a % 10, 9, colour, pixels);
  drawDigit(b / 10, 18, colour, pixels);
  drawDigit(b % 10, 25, colour, pixels);
}

// Two digits centred on the panel, for anything that is a small number.
void drawPair(uint8_t value, uint32_t colour, uint32_t* pixels) {
  const uint8_t clamped = value > 99 ? 99 : value;
  drawDigit(clamped / 10, 10, colour, pixels);
  drawDigit(clamped % 10, 17, colour, pixels);
}

void writeText(RenderResult& result, const char* text) {
  uint8_t index = 0;
  while (text[index] != '\0' && index < sizeof(result.text) - 1) {
    result.text[index] = text[index];
    index++;
  }
  result.text[index] = '\0';
}

void writePairText(RenderResult& result, uint16_t left, uint16_t right) {
  const uint8_t a = left > 99 ? 99 : static_cast<uint8_t>(left);
  const uint8_t b = right > 99 ? 99 : static_cast<uint8_t>(right);
  result.text[0] = static_cast<char>('0' + a / 10);
  result.text[1] = static_cast<char>('0' + a % 10);
  result.text[2] = '-';
  result.text[3] = static_cast<char>('0' + b / 10);
  result.text[4] = static_cast<char>('0' + b % 10);
  result.text[5] = '\0';
}

void writeNumberText(RenderResult& result, uint8_t value) {
  const uint8_t clamped = value > 99 ? 99 : value;
  result.text[0] = static_cast<char>('0' + clamped / 10);
  result.text[1] = static_cast<char>('0' + clamped % 10);
  result.text[2] = '\0';
}

void writeLabeledNumber(RenderResult& result, const char* label, uint16_t number) {
  uint8_t index = 0;
  while (label[index] != '\0' && index < sizeof(result.text) - 1) {
    result.text[index] = label[index];
    index++;
  }
  if (index < sizeof(result.text) - 1) result.text[index++] = ' ';
  const uint16_t shown = number > 99 ? 99 : number;
  if (shown >= 10) {
    if (index < sizeof(result.text) - 1) {
      result.text[index++] = static_cast<char>('0' + shown / 10);
    }
  }
  if (index < sizeof(result.text) - 1) {
    result.text[index++] = static_cast<char>('0' + shown % 10);
  }
  result.text[index] = '\0';
}

void writeSecondsText(RenderResult& result, uint32_t shown, uint8_t count, uint8_t prefix) {
  uint32_t value = shown;
  uint8_t digits[4] = {0};
  uint8_t n = count;
  while (n > 0) {
    n--;
    digits[n] = static_cast<uint8_t>(value % 10);
    value /= 10;
  }
  for (uint8_t index = 0; index < count && prefix + index < sizeof(result.text) - 1; index++) {
    result.text[prefix + index] = static_cast<char>('0' + digits[index]);
  }
  result.text[prefix + count] = '\0';
}

uint8_t secondsDigits(uint32_t shown, uint8_t* digits) {
  uint8_t count = 0;
  if (shown == 0) {
    digits[count++] = 0;
    return count;
  }
  uint8_t reversed[4] = {0};
  uint8_t n = 0;
  uint32_t value = shown;
  while (value > 0 && n < 4) {
    reversed[n++] = static_cast<uint8_t>(value % 10);
    value /= 10;
  }
  while (n > 0) digits[count++] = reversed[--n];
  return count;
}

void drawRightSeconds(uint32_t totalSeconds, uint8_t top, uint32_t colour, uint32_t* pixels,
                      RenderResult& result, uint8_t textAt) {
  uint32_t shown = totalSeconds > 9999 ? 9999 : totalSeconds;
  uint8_t digits[4] = {0};
  const uint8_t count = secondsDigits(shown, digits);
  uint8_t left = static_cast<uint8_t>(CLOCK_ONES_LEFT - (count - 1) * (DIGIT_WIDTH + DIGIT_GAP));
  for (uint8_t index = 0; index < count; index++) {
    drawDigitAt(digits[index], left, top, colour, pixels);
    left = static_cast<uint8_t>(left + DIGIT_WIDTH + DIGIT_GAP);
  }
  writeSecondsText(result, shown, count, textAt);
}

void prefixGroupText(RenderResult& result, const char* letters) {
  char rest[16];
  uint8_t index = 0;
  while (result.text[index] != '\0' && index < sizeof(rest) - 1) {
    rest[index] = result.text[index];
    index++;
  }
  rest[index] = '\0';
  result.text[0] = letters[0];
  result.text[1] = letters[1];
  result.text[2] = ' ';
  uint8_t out = 3;
  for (uint8_t cursor = 0; rest[cursor] != '\0' && out < sizeof(result.text) - 1; cursor++) {
    result.text[out++] = rest[cursor];
  }
  result.text[out] = '\0';
}

void drawMmSs(uint32_t totalSeconds, uint32_t colour, uint32_t* pixels, RenderResult& result) {
  const uint32_t minutes = (totalSeconds / 60) % 100;
  const uint32_t seconds = totalSeconds % 60;
  drawDigit(static_cast<uint8_t>(minutes / 10), 2, colour, pixels);
  drawDigit(static_cast<uint8_t>(minutes % 10), 9, colour, pixels);
  drawDigit(static_cast<uint8_t>(seconds / 10), 18, colour, pixels);
  drawDigit(static_cast<uint8_t>(seconds % 10), 25, colour, pixels);
  drawColon(colour, pixels);
  result.text[0] = static_cast<char>('0' + minutes / 10);
  result.text[1] = static_cast<char>('0' + minutes % 10);
  result.text[2] = ':';
  result.text[3] = static_cast<char>('0' + seconds / 10);
  result.text[4] = static_cast<char>('0' + seconds % 10);
  result.text[5] = '\0';
}

void drawEndLabel(const RenderRequest& request, uint32_t colour, uint32_t* pixels,
                  RenderResult& result) {
  const bool scoring = request.phase == Core::Phase::Scoring;
  drawWideWord(scoring ? "SCORE" : "END", 0, colour, pixels);
  const uint16_t shown = request.endNumber > 99 ? 99 : request.endNumber;
  drawDigitAt(static_cast<uint8_t>(shown / 10), 10, 5, colour, pixels);
  drawDigitAt(static_cast<uint8_t>(shown % 10), 17, 5, colour, pixels);
  writeLabeledNumber(result, scoring ? "Scoring" : "End", shown);
}

void drawClock(const RenderRequest& request, uint32_t colour, uint32_t* pixels,
               RenderResult& result) {
  if (request.showEndLabels && endLabelPhase(request.phase)) {
    drawEndLabel(request, colour, pixels, result);
    return;
  }

  const uint32_t totalSeconds = (request.remainingMs + 999) / 1000;
  const bool group = showGroup(request);
  const char* letters = groupLetters(request.detail);
  const uint8_t top = (group && !request.abcdVertical) ? 0 : DIGIT_TOP;

  if (request.clockSeconds) {
    drawRightSeconds(totalSeconds, top, colour, pixels, result, 0);
  } else {
    drawMmSs(totalSeconds, colour, pixels, result);
  }

  if (!group) return;
  const uint32_t letterColour = groupColour(request, colour);
  if (request.abcdVertical) {
    drawGroupVertical(letters, letterColour, pixels);
  } else {
    drawGroupUnder(letters, letterColour, pixels);
  }
  prefixGroupText(result, letters);
}

}  // namespace

uint16_t ledIndex(uint8_t x, uint8_t y) {
  const uint8_t tileY = y / PANEL_ROWS;
  const uint8_t panel = PANEL_ORDER[tileY];
  uint8_t localX = x % PANEL_COLUMNS;
  uint8_t localY = y % PANEL_ROWS;
  if (tileY == 0) {
    localX = PANEL_COLUMNS - 1 - localX;
    localY = PANEL_ROWS - 1 - localY;
  }
  if (localX & 1) localY = PANEL_ROWS - 1 - localY;
  return panel * PANEL_COLUMNS * PANEL_ROWS + localX * PANEL_ROWS + localY;
}

uint32_t colourFor(Core::Light light) {
  switch (light) {
    case Core::Light::Red: return COLOUR_RED;
    case Core::Light::Green: return COLOUR_GREEN;
    case Core::Light::Yellow: return COLOUR_YELLOW;
    case Core::Light::Off: return COLOUR_IDLE;
  }
  return COLOUR_IDLE;
}

uint32_t groupColour(const RenderRequest& request, uint32_t timerColour) {
  if (request.abcdFollowTimer) return timerColour;
  return request.abcdColour;
}

namespace {

int hexNibble(char character) {
  if (character >= '0' && character <= '9') return character - '0';
  if (character >= 'a' && character <= 'f') return character - 'a' + 10;
  if (character >= 'A' && character <= 'F') return character - 'A' + 10;
  return -1;
}

}  // namespace

uint32_t parseCssColour(const char* text, uint32_t fallback) {
  if (text == nullptr) return fallback;
  if (text[0] == '#') text++;
  uint32_t value = 0;
  for (uint8_t index = 0; index < 6; index++) {
    const int nibble = hexNibble(text[index]);
    if (nibble < 0) return fallback;
    value = (value << 4) | static_cast<uint32_t>(nibble);
  }
  return value;
}

void formatCssColour(uint32_t colour, char* out, uint8_t outSize) {
  if (out == nullptr || outSize < 8) return;
  static const char* hex = "0123456789abcdef";
  out[0] = '#';
  uint32_t value = colour & 0xFFFFFFu;
  for (uint8_t index = 0; index < 6; index++) {
    const uint8_t shift = static_cast<uint8_t>((5 - index) * 4);
    out[index + 1] = hex[(value >> shift) & 0xF];
  }
  out[7] = '\0';
}

bool contentAvailable(Core::DisplayContent content) {
  switch (content) {
    case Core::DisplayContent::Clock:
    case Core::DisplayContent::ClockAndEnd:
    case Core::DisplayContent::ArrowCount:
    case Core::DisplayContent::Blank:
    case Core::DisplayContent::Score:
    case Core::DisplayContent::SetPoints:
    case Core::DisplayContent::Shooter:
      return true;
  }
  return false;
}

void fillFromSnapshot(RenderRequest& request, const Core::StateSnapshot& state) {
  request.content = state.display;
  request.light = state.light;
  request.phase = state.phase;
  request.remainingMs = state.remainingMs;
  request.endNumber = state.endNumber;
  request.arrowsShot = state.arrowsShot;
  request.arrowsPerEnd = state.arrowsPerEnd;
  request.score[0] = state.score[0];
  request.score[1] = state.score[1];
  request.setPoints[0] = state.setPoints[0];
  request.setPoints[1] = state.setPoints[1];
  request.shooter = state.shooter;
  request.detail = state.detail;
  request.details = state.details;
}

RenderResult renderFrame(const RenderRequest& request, uint32_t* pixels) {
  for (uint16_t index = 0; index < PIXEL_COUNT; index++) pixels[index] = 0;

  RenderResult result;
  const uint32_t colour = colourFor(request.light);
  const Core::DisplayContent content =
      contentAvailable(request.content) ? request.content : Core::DisplayContent::Clock;

  switch (content) {
    case Core::DisplayContent::Blank:
      writeText(result, "");
      break;

    case Core::DisplayContent::ClockAndEnd:
      if (request.showEndLabels) {
        drawEndLabel(request, colour, pixels, result);
      } else {
        drawPair(static_cast<uint8_t>(request.endNumber > 99 ? 99 : request.endNumber), colour, pixels);
        writeNumberText(result, static_cast<uint8_t>(request.endNumber > 99 ? 99 : request.endNumber));
      }
      break;

    case Core::DisplayContent::ArrowCount:
      drawDigit(request.arrowsShot > 9 ? 9 : request.arrowsShot, 7, colour, pixels);
      drawSeparator(colour, pixels);
      drawDigit(request.arrowsPerEnd > 9 ? 9 : request.arrowsPerEnd, 20, colour, pixels);
      result.text[0] = static_cast<char>('0' + (request.arrowsShot > 9 ? 9 : request.arrowsShot));
      result.text[1] = '/';
      result.text[2] = static_cast<char>('0' + (request.arrowsPerEnd > 9 ? 9 : request.arrowsPerEnd));
      result.text[3] = '\0';
      break;

    case Core::DisplayContent::Score:
      drawTwoPairs(request.score[0], request.score[1], colour, pixels);
      writePairText(result, request.score[0], request.score[1]);
      break;

    case Core::DisplayContent::SetPoints:
      drawTwoPairs(request.setPoints[0], request.setPoints[1], colour, pixels);
      writePairText(result, request.setPoints[0], request.setPoints[1]);
      break;

    case Core::DisplayContent::Shooter:
      drawPair(request.shooter, colour, pixels);
      writeNumberText(result, request.shooter);
      break;

    case Core::DisplayContent::Clock:
    default:
      // Round up, so the panel reads 1 while any part of the last second is
      // left and reaches 0 exactly when shooting time is over.
      drawClock(request, colour, pixels, result);
      break;
  }

  // A cheap order-sensitive hash. Enough to say "the panel is showing a
  // different frame now" in the log without printing 512 pixels.
  uint32_t checksum = 2166136261u;
  for (uint16_t index = 0; index < PIXEL_COUNT; index++) {
    if (pixels[index] != 0) result.litPixels++;
    checksum ^= pixels[index] + index;
    checksum *= 16777619u;
  }
  result.checksum = checksum;
  return result;
}

}  // namespace DisplayLogic
