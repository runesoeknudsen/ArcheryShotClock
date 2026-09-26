#include "smooth_draw.h"

#include <math.h>

#include "font_face.h"
#include "small_font.h"

namespace DisplayLogic {
namespace {

constexpr uint8_t DIGIT_WIDTH = 5;
constexpr uint8_t DIGIT_HEIGHT = 11;
constexpr uint8_t DIGIT_GAP = 1;
constexpr uint8_t DIGIT_TOP = (ROWS - DIGIT_HEIGHT) / 2;
constexpr uint8_t GROUP_LEFT = 0;
constexpr uint8_t LETTER_GAP = 1;
constexpr float LETTER_SPACE = 0.75f;
constexpr uint8_t ELEMENT_GAP = 1;
constexpr float SHOOT_GROUP_H = 7.0f;
constexpr float SHOOT_DIGIT_H = 15.0f;
constexpr float SHOOT_TOP = 0.5f;
constexpr float EDGE_PAD = 0.25f;

struct Canvas {
  uint32_t* pixels = nullptr;
  uint16_t destX = 0;
  uint16_t destY = 0;
  uint16_t destW = 0;
  uint16_t destH = 0;
  uint16_t columns = 0;
  uint16_t rows = 0;
  float sx = 1.0f;
  float sy = 1.0f;
};

struct GlyphMap {
  float scale = 1.0f;
  float ox = 0;
  float oy = 0;
};

uint8_t mix8(uint8_t a, uint8_t b, uint8_t t) {
  return static_cast<uint8_t>((static_cast<uint16_t>(a) * (255 - t) + static_cast<uint16_t>(b) * t + 127) /
                              255);
}

uint32_t mixColour(uint32_t a, uint32_t b, uint8_t t) {
  if (a == b || t == 0) return a;
  if (t == 255) return b;
  const uint32_t r = mix8(static_cast<uint8_t>(a >> 16), static_cast<uint8_t>(b >> 16), t);
  const uint32_t g = mix8(static_cast<uint8_t>(a >> 8), static_cast<uint8_t>(b >> 8), t);
  const uint32_t blue = mix8(static_cast<uint8_t>(a), static_cast<uint8_t>(b), t);
  return (r << 16) | (g << 8) | blue;
}

uint16_t luma(uint32_t colour) {
  return static_cast<uint16_t>(((colour >> 16) & 0xFFu) + ((colour >> 8) & 0xFFu) + (colour & 0xFFu));
}

float clampf(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

float smoothstep(float edge0, float edge1, float x) {
  const float span = edge1 - edge0;
  if (span <= 1e-6f) return x >= edge1 ? 1.0f : 0.0f;
  const float t = clampf((x - edge0) / span, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float distToSeg(float px, float py, float x0, float y0, float x1, float y1) {
  const float vx = x1 - x0;
  const float vy = y1 - y0;
  const float wx = px - x0;
  const float wy = py - y0;
  const float len2 = vx * vx + vy * vy;
  float t = 0.0f;
  if (len2 > 1e-8f) t = (wx * vx + wy * vy) / len2;
  t = clampf(t, 0.0f, 1.0f);
  const float dx = px - (x0 + t * vx);
  const float dy = py - (y0 + t * vy);
  return sqrtf(dx * dx + dy * dy);
}

float isLeft(float x0, float y0, float x1, float y1, float px, float py) {
  return (x1 - x0) * (py - y0) - (px - x0) * (y1 - y0);
}

float outlineSdf(const FontFace& face, const FontGlyph& glyph, float px, float py) {
  float best = 1e9f;
  int winding = 0;
  for (uint8_t contour = 0; contour < glyph.contours; contour++) {
    const GlyphContour& ring = face.contours[glyph.contour0 + contour];
    if (ring.count < 2) continue;
    for (uint16_t index = 0; index < ring.count; index++) {
      const GlyphPoint& a = face.points[ring.first + index];
      const GlyphPoint& b = face.points[ring.first + ((index + 1) % ring.count)];
      const float x0 = static_cast<float>(a.x);
      const float y0 = static_cast<float>(a.y);
      const float x1 = static_cast<float>(b.x);
      const float y1 = static_cast<float>(b.y);
      const float dist = distToSeg(px, py, x0, y0, x1, y1);
      if (dist < best) best = dist;
      if (y0 <= py) {
        if (y1 > py && isLeft(x0, y0, x1, y1, px, py) > 0.0f) winding++;
      } else if (y1 <= py && isLeft(x0, y0, x1, y1, px, py) < 0.0f) {
        winding--;
      }
    }
  }
  return winding == 0 ? best : -best;
}

GlyphMap mapGlyph(float left, float top, float width, float height, const FontFace& face,
                  const FontGlyph& glyph) {
  float cap = static_cast<float>(face.capHeight);
  if (cap < 1.0f) cap = static_cast<float>(face.unitsPerEm) * 0.72f;
  float faceW = static_cast<float>(face.maxInkWidth);
  if (faceW < 1.0f) faceW = cap;
  float inkW = static_cast<float>(glyph.maxX - glyph.minX);
  if (inkW < 1.0f) inkW = 1.0f;
  const float pad = 0.06f;
  const float sH = (height * (1.0f - 2.0f * pad)) / cap;
  const float sW = (width * (1.0f - 2.0f * pad)) / faceW;
  float scale = sH < sW ? sH : sW;
  if (scale < 1e-6f) scale = 1e-6f;
  GlyphMap mapped;
  mapped.scale = scale;
  mapped.ox = left + (width - inkW * scale) * 0.5f - static_cast<float>(glyph.minX) * scale;
  const float capLine = top + (height - cap * scale) * 0.5f;
  mapped.oy = capLine + cap * scale;
  return mapped;
}

void stampGlyph(const Canvas& canvas, uint8_t code, float left, float top, float width, float height,
                uint32_t colour) {
  if (canvas.pixels == nullptr || canvas.destW == 0 || canvas.destH == 0) return;
  const FontFace& face = activeFontFace();
  const FontGlyph* glyph = findGlyph(face, code);
  if (glyph == nullptr) return;
  const GlyphMap mapped = mapGlyph(left, top, width, height, face, *glyph);

  const float x0u = mapped.ox + static_cast<float>(glyph->minX) * mapped.scale;
  const float x1u = mapped.ox + static_cast<float>(glyph->maxX) * mapped.scale;
  const float y0u = mapped.oy - static_cast<float>(glyph->maxY) * mapped.scale;
  const float y1u = mapped.oy - static_cast<float>(glyph->minY) * mapped.scale;
  float aa = 0.50f * (canvas.sx < canvas.sy ? canvas.sx : canvas.sy);
  if (aa < 0.90f) aa = 0.90f;
  if (aa > 1.70f) aa = 1.70f;
  const float pad = (aa + 1.2f) / (canvas.sx < canvas.sy ? canvas.sx : canvas.sy);

  int x0 = static_cast<int>((x0u - pad) * canvas.sx + static_cast<float>(canvas.destX));
  int y0 = static_cast<int>((y0u - pad) * canvas.sy + static_cast<float>(canvas.destY));
  int x1 = static_cast<int>((x1u + pad) * canvas.sx + static_cast<float>(canvas.destX) + 0.999f);
  int y1 = static_cast<int>((y1u + pad) * canvas.sy + static_cast<float>(canvas.destY) + 0.999f);
  const int clipX0 = canvas.destX;
  const int clipY0 = canvas.destY;
  const int clipX1 = static_cast<int>(canvas.destX + canvas.destW);
  const int clipY1 = static_cast<int>(canvas.destY + canvas.destH);
  if (x0 < clipX0) x0 = clipX0;
  if (y0 < clipY0) y0 = clipY0;
  if (x1 > clipX1) x1 = clipX1;
  if (y1 > clipY1) y1 = clipY1;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > static_cast<int>(canvas.columns)) x1 = canvas.columns;
  if (y1 > static_cast<int>(canvas.rows)) y1 = canvas.rows;
  if (x0 >= x1 || y0 >= y1) return;

  const float invS = 1.0f / mapped.scale;
  for (int py = y0; py < y1; py++) {
    for (int px = x0; px < x1; px++) {
      const float ux = (static_cast<float>(px) + 0.5f - static_cast<float>(canvas.destX)) / canvas.sx;
      const float uy = (static_cast<float>(py) + 0.5f - static_cast<float>(canvas.destY)) / canvas.sy;
      const float fx = (ux - mapped.ox) * invS;
      const float fy = (mapped.oy - uy) * invS;
      const float sdf = outlineSdf(face, *glyph, fx, fy) * mapped.scale *
                        (0.5f * (canvas.sx + canvas.sy));
      const float coverage = 1.0f - smoothstep(-aa, aa, sdf);
      if (coverage <= 0.004f) continue;
      const uint8_t t = static_cast<uint8_t>(coverage * 255.0f + 0.5f);
      if (t == 0) continue;
      const uint32_t mixed = mixColour(0, colour, t);
      uint32_t& dest = canvas.pixels[logicalIndex(static_cast<uint16_t>(px), static_cast<uint16_t>(py),
                                                  canvas.columns)];
      if (luma(mixed) > luma(dest)) dest = mixed;
    }
  }
}

float faceScale(const FontFace& face, float width, float height) {
  FontGlyph dummy;
  dummy.minX = 0;
  dummy.maxX = 1;
  return mapGlyph(0.0f, 0.0f, width, height, face, dummy).scale;
}

float inkWidth(const FontGlyph& glyph, float scale) {
  float inkW = static_cast<float>(glyph.maxX - glyph.minX);
  if (inkW < 1.0f) inkW = 1.0f;
  return inkW * scale;
}

float packedWidth(uint8_t code, float boxW, float boxH) {
  const FontFace& face = activeFontFace();
  const FontGlyph* glyph = findGlyph(face, code);
  if (glyph == nullptr) return boxW;
  return inkWidth(*glyph, faceScale(face, boxW, boxH));
}

void stampPacked(const Canvas& canvas, uint8_t code, float inkLeft, float top, float boxW, float boxH,
                 uint32_t colour) {
  const FontFace& face = activeFontFace();
  const FontGlyph* glyph = findGlyph(face, code);
  if (glyph == nullptr) return;
  const float scale = faceScale(face, boxW, boxH);
  const float left = inkLeft - (boxW - inkWidth(*glyph, scale)) * 0.5f;
  stampGlyph(canvas, code, left, top, boxW, boxH, colour);
}

float boxWidthForScale(float scale) {
  const FontFace& face = activeFontFace();
  float faceW = static_cast<float>(face.maxInkWidth);
  if (faceW < 1.0f) faceW = static_cast<float>(face.capHeight);
  if (faceW < 1.0f) faceW = 1.0f;
  return scale * faceW / 0.88f;
}

float heightScale(float boxH) { return faceScale(activeFontFace(), 1000.0f, boxH); }

float groupColumnWidth() { return boxWidthForScale(heightScale(SHOOT_GROUP_H)); }

bool isDigitCode(uint8_t code) { return code >= '0' && code <= '9'; }

float maxDigitInk(const FontFace& face, float scale) {
  float best = 0.0f;
  for (uint8_t digit = 0; digit < 10; digit++) {
    const FontGlyph* glyph = findGlyph(face, static_cast<uint8_t>('0' + digit));
    if (glyph == nullptr) continue;
    const float width = inkWidth(*glyph, scale);
    if (width > best) best = width;
  }
  return best > 1.0f ? best : scale;
}

void fitLine(const uint8_t* codes, uint8_t count, float boxH, float availableW, float extraGap,
             bool tabularDigits, float* outBoxW, float* outWidths, float* outTotal) {
  const FontFace& face = activeFontFace();
  const float sH = heightScale(boxH);
  const float digitCell = tabularDigits ? maxDigitInk(face, sH) : 0.0f;
  float inkSum = 0.0f;
  float inks[8] = {};
  const uint8_t used = count < 8 ? count : 8;
  for (uint8_t index = 0; index < used; index++) {
    if (tabularDigits && isDigitCode(codes[index])) {
      inks[index] = digitCell;
    } else {
      const FontGlyph* glyph = findGlyph(face, codes[index]);
      inks[index] = glyph == nullptr ? boxH * 0.5f : inkWidth(*glyph, sH);
    }
    inkSum += inks[index];
  }
  const float gaps = used > 1 ? LETTER_SPACE * static_cast<float>(used - 1) + extraGap : 0.0f;
  float scale = sH;
  if (inkSum + gaps > availableW && inkSum > 0.1f) {
    float room = availableW - gaps;
    if (room < 1.0f) room = 1.0f;
    scale = sH * room / inkSum;
  }
  *outBoxW = boxWidthForScale(scale);
  *outTotal = 0.0f;
  const float factor = sH > 1e-6f ? scale / sH : 1.0f;
  for (uint8_t index = 0; index < used; index++) {
    outWidths[index] = inks[index] * factor;
    if (index > 0) *outTotal += LETTER_SPACE;
    *outTotal += outWidths[index];
  }
  *outTotal += extraGap;
}

void stampFitted(const Canvas& canvas, const uint8_t* codes, uint8_t count, float left, float top,
                 float boxW, float boxH, const float* widths, uint8_t extraAt, uint32_t colour) {
  float cursor = left;
  for (uint8_t index = 0; index < count; index++) {
    // Digits share a left rail in the tabular slot so a 1 does not slide
    // toward the centre when it replaces a 0. Letters and the colon stay
    // optically centred in their own advance.
    float inkLeft = cursor;
    if (!isDigitCode(codes[index])) {
      const float inkW = packedWidth(codes[index], boxW, boxH);
      if (widths[index] > inkW) inkLeft += (widths[index] - inkW) * 0.5f;
    }
    stampPacked(canvas, codes[index], inkLeft, top, boxW, boxH, colour);
    cursor += widths[index] + LETTER_SPACE;
    if (extraAt != 0 && index + 1 == extraAt) cursor += LETTER_SPACE;
  }
}

void stackTwoLines(float preferTop, float preferBot, float* topH, float* botH, float* botTop) {
  const float gap = static_cast<float>(ELEMENT_GAP);
  const float leftover = static_cast<float>(ROWS) - preferTop - preferBot - gap;
  if (leftover <= 0.0f) {
    *topH = preferTop;
    *botH = preferBot;
    *botTop = preferTop + gap;
    return;
  }
  const float span = preferTop + preferBot;
  *topH = preferTop + leftover * (preferTop / span);
  *botH = preferBot + leftover * (preferBot / span);
  *botTop = *topH + gap;
}

void drawDigit(const Canvas& canvas, uint8_t value, float left, float top, bool compact, uint32_t colour) {
  if (value > 9) return;
  const float height = compact ? static_cast<float>(SmallFont::DIGIT_HEIGHT) : DIGIT_HEIGHT;
  stampGlyph(canvas, static_cast<uint8_t>('0' + value), left, top, DIGIT_WIDTH, height, colour);
}

void drawColon(const Canvas& canvas, float origin, float top, bool compact, uint32_t colour) {
  const float height = compact ? static_cast<float>(SmallFont::DIGIT_HEIGHT) : DIGIT_HEIGHT;
  stampGlyph(canvas, ':', origin + 12.0f, top, 3.0f, height, colour);
}

void drawSeparator(const Canvas& canvas, uint32_t colour) {
  stampGlyph(canvas, '-', 13.0f, DIGIT_TOP, 6.0f, DIGIT_HEIGHT, colour);
}

void drawWideWord(const Canvas& canvas, const char* word, float top, float boxH, uint32_t colour) {
  uint8_t codes[8] = {};
  uint8_t count = 0;
  while (word[count] != '\0' && count < 8) {
    codes[count] = static_cast<uint8_t>(word[count]);
    count++;
  }
  if (count == 0) return;
  float widths[8] = {};
  float boxW = 0.0f;
  float total = 0.0f;
  fitLine(codes, count, boxH, static_cast<float>(COLUMNS) - EDGE_PAD, 0.0f, false, &boxW, widths,
          &total);
  const float left = total >= static_cast<float>(COLUMNS) ? 0.0f
                                                         : (static_cast<float>(COLUMNS) - total) * 0.5f;
  stampFitted(canvas, codes, count, left, top, boxW, boxH, widths, 0, colour);
}

void drawNarrowLetter(const Canvas& canvas, char letter, float left, float top, uint32_t colour) {
  stampGlyph(canvas, static_cast<uint8_t>(letter), left, top, SmallFont::NARROW_WIDTH,
             SmallFont::NARROW_HEIGHT, colour);
}

uint8_t minTimeLeft(bool groupVertical, bool wideGroup) {
  if (!groupVertical) return 0;
  const uint8_t groupW = wideGroup ? SmallFont::WIDE_WIDTH : SmallFont::NARROW_WIDTH;
  return static_cast<uint8_t>(GROUP_LEFT + groupW + ELEMENT_GAP);
}

const char* groupLetters(uint8_t detail) {
  switch (detail) {
    case 2: return "CD";
    case 3: return "EF";
    case 4: return "GH";
    default: return "AB";
  }
}

bool showGroup(const RenderRequest& request) {
  return request.showAbcd && request.details > 1 && request.phase != Core::Phase::Break;
}

bool endLabelPhase(Core::Phase phase) {
  return phase == Core::Phase::Finished || phase == Core::Phase::Scoring;
}

void drawMmSs(const Canvas& canvas, uint32_t totalSeconds, uint32_t colour, float top, float boxH,
              float origin, float availableW) {
  const uint32_t minutes = (totalSeconds / 60) % 100;
  const uint32_t seconds = totalSeconds % 60;
  const uint8_t codes[5] = {
      static_cast<uint8_t>('0' + minutes / 10), static_cast<uint8_t>('0' + minutes % 10), ':',
      static_cast<uint8_t>('0' + seconds / 10), static_cast<uint8_t>('0' + seconds % 10)};
  float widths[5] = {};
  float boxW = 0.0f;
  float total = 0.0f;
  fitLine(codes, 5, boxH, availableW, 0.0f, true, &boxW, widths, &total);
  float left = origin + (availableW - total) * 0.5f;
  if (left < origin) left = origin;
  stampFitted(canvas, codes, 5, left, top, boxW, boxH, widths, 0, colour);
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
  while (value > 0 && n < 3) {
    reversed[n++] = static_cast<uint8_t>(value % 10);
    value /= 10;
  }
  while (n > 0) digits[count++] = reversed[--n];
  return count;
}

void drawRightSeconds(const Canvas& canvas, uint32_t totalSeconds, float top, uint32_t colour,
                      float minLeft, bool compact) {
  uint32_t shown = totalSeconds > 999 ? 999 : totalSeconds;
  uint8_t digits[4] = {0};
  const uint8_t count = secondsDigits(shown, digits);
  float left = static_cast<float>(CLOCK_ONES_LEFT - (count - 1) * (DIGIT_WIDTH + DIGIT_GAP));
  if (left < minLeft) left = minLeft;
  for (uint8_t index = 0; index < count; index++) {
    drawDigit(canvas, digits[index], left, top, compact, colour);
    left += DIGIT_WIDTH + DIGIT_GAP;
  }
}

void drawEndLabel(const Canvas& canvas, const RenderRequest& request, uint32_t colour) {
  const bool scoring = request.phase == Core::Phase::Scoring;
  float wordH = 0.0f;
  float timeH = 0.0f;
  float timeTop = 0.0f;
  stackTwoLines(static_cast<float>(SmallFont::WIDE_HEIGHT), static_cast<float>(SmallFont::DIGIT_HEIGHT),
                &wordH, &timeH, &timeTop);
  drawWideWord(canvas, scoring ? "SCORE" : "END", 0.0f, wordH, colour);
  const uint16_t shown = request.endNumber > 99 ? 99 : request.endNumber;
  const uint8_t codes[2] = {static_cast<uint8_t>('0' + shown / 10),
                            static_cast<uint8_t>('0' + shown % 10)};
  float widths[2] = {};
  float boxW = 0.0f;
  float total = 0.0f;
  fitLine(codes, 2, timeH, static_cast<float>(COLUMNS) - EDGE_PAD, 0.0f, true, &boxW, widths,
          &total);
  const float left = (static_cast<float>(COLUMNS) - total) * 0.5f;
  stampFitted(canvas, codes, 2, left, timeTop, boxW, timeH, widths, 0, colour);
}

void drawGroup(const Canvas& canvas, const char* letters, uint32_t colour, bool vertical, bool wide) {
  if (vertical) {
    if (wide) {
      stampGlyph(canvas, static_cast<uint8_t>(letters[0]), GROUP_LEFT, 2.0f, SmallFont::WIDE_WIDTH,
                 SmallFont::WIDE_HEIGHT, colour);
      stampGlyph(canvas, static_cast<uint8_t>(letters[1]), GROUP_LEFT, 9.0f, SmallFont::WIDE_WIDTH,
                 SmallFont::WIDE_HEIGHT, colour);
      return;
    }
    drawNarrowLetter(canvas, letters[0], GROUP_LEFT, 2.0f, colour);
    drawNarrowLetter(canvas, letters[1], GROUP_LEFT, 9.0f, colour);
    return;
  }
  const float boxW = static_cast<float>(SmallFont::NARROW_WIDTH);
  const float boxH = static_cast<float>(SmallFont::NARROW_HEIGHT);
  const float firstW = packedWidth(static_cast<uint8_t>(letters[0]), boxW, boxH);
  const float total = firstW + LETTER_SPACE + packedWidth(static_cast<uint8_t>(letters[1]), boxW, boxH);
  const float left = static_cast<float>(CLOCK_ONES_LEFT + DIGIT_WIDTH) - total;
  stampPacked(canvas, static_cast<uint8_t>(letters[0]), left, 11.0f, boxW, boxH, colour);
  stampPacked(canvas, static_cast<uint8_t>(letters[1]), left + firstW + LETTER_SPACE, 11.0f, boxW,
              boxH, colour);
}

void drawLargeSeconds(const Canvas& canvas, uint32_t totalSeconds, uint32_t colour, bool withGroup) {
  uint32_t shown = totalSeconds > 999 ? 999 : totalSeconds;
  uint8_t digits[3] = {0};
  const uint8_t count = secondsDigits(shown, digits);
  uint8_t codes[3] = {};
  for (uint8_t index = 0; index < count; index++) {
    codes[index] = static_cast<uint8_t>('0' + digits[index]);
  }
  const float groupW = withGroup ? groupColumnWidth() + LETTER_SPACE : 0.0f;
  const float available = static_cast<float>(COLUMNS) - groupW - EDGE_PAD;
  float widths[3] = {};
  float boxW = 0.0f;
  float total = 0.0f;
  fitLine(codes, count, SHOOT_DIGIT_H, available, 0.0f, true, &boxW, widths, &total);
  float left = static_cast<float>(COLUMNS) - total - EDGE_PAD;
  if (left < groupW) left = groupW;
  stampFitted(canvas, codes, count, left, SHOOT_TOP, boxW, SHOOT_DIGIT_H, widths, 0, colour);
}

void drawLargeGroup(const Canvas& canvas, const char* letters, uint32_t colour) {
  const float boxW = groupColumnWidth();
  stampGlyph(canvas, static_cast<uint8_t>(letters[0]), 0.0f, SHOOT_TOP, boxW, SHOOT_GROUP_H, colour);
  stampGlyph(canvas, static_cast<uint8_t>(letters[1]), 0.0f, SHOOT_TOP + SHOOT_GROUP_H + LETTER_SPACE,
             boxW, SHOOT_GROUP_H, colour);
}

void drawClock(const Canvas& canvas, const RenderRequest& request, uint32_t colour) {
  if (request.showEndLabels && endLabelPhase(request.phase)) {
    drawEndLabel(canvas, request, colour);
    return;
  }

  const bool seconds = secondsClock(request);
  const uint32_t totalSeconds =
      seconds ? countdownSeconds(request.remainingMs) : (request.remainingMs + 999) / 1000;
  const bool group = showGroup(request);
  const bool stackGroup = group && (request.abcdVertical || shotCountdown(request.phase));
  const bool groupUnder = group && !stackGroup;
  const char* letters = groupLetters(request.detail);
  const bool compactTime = request.phase == Core::Phase::Break || groupUnder;
  const float top = compactTime ? 0.0f : DIGIT_TOP;
  const float origin = stackGroup ? static_cast<float>(minTimeLeft(true, seconds)) : 2.0f;

  if (request.phase == Core::Phase::Break) {
    float wordH = 0.0f;
    float timeH = 0.0f;
    float timeTop = 0.0f;
    stackTwoLines(static_cast<float>(SmallFont::WIDE_HEIGHT), static_cast<float>(SmallFont::DIGIT_HEIGHT),
                  &wordH, &timeH, &timeTop);
    drawWideWord(canvas, "BREAK", 0.0f, wordH, colour);
    drawMmSs(canvas, totalSeconds, colour, timeTop, timeH, 0.0f, static_cast<float>(COLUMNS) - EDGE_PAD);
    return;
  }

  if (seconds) {
    drawLargeSeconds(canvas, totalSeconds, colour, stackGroup);
    if (group) drawLargeGroup(canvas, letters, groupColour(request, colour));
    return;
  }

  const float boxH = compactTime ? static_cast<float>(SmallFont::DIGIT_HEIGHT) : DIGIT_HEIGHT;
  const float available = static_cast<float>(COLUMNS) - origin - EDGE_PAD;
  drawMmSs(canvas, totalSeconds, colour, top, boxH, origin, available);
  if (!group) return;
  drawGroup(canvas, letters, groupColour(request, colour), stackGroup, false);
}

void drawTwoPairs(const Canvas& canvas, uint16_t left, uint16_t right, uint32_t colour) {
  const uint8_t a = left > 99 ? 99 : static_cast<uint8_t>(left);
  const uint8_t b = right > 99 ? 99 : static_cast<uint8_t>(right);
  const uint8_t codes[4] = {static_cast<uint8_t>('0' + a / 10), static_cast<uint8_t>('0' + a % 10),
                            static_cast<uint8_t>('0' + b / 10), static_cast<uint8_t>('0' + b % 10)};
  float widths[4] = {};
  float boxW = 0.0f;
  float total = 0.0f;
  const float boxH = static_cast<float>(DIGIT_HEIGHT);
  fitLine(codes, 4, boxH, static_cast<float>(COLUMNS) - EDGE_PAD, LETTER_SPACE, true, &boxW, widths,
          &total);
  const float cursor = (static_cast<float>(COLUMNS) - total) * 0.5f;
  stampFitted(canvas, codes, 4, cursor, DIGIT_TOP, boxW, boxH, widths, 2, colour);
}

void drawPair(const Canvas& canvas, uint8_t value, uint32_t colour) {
  const uint8_t clamped = value > 99 ? 99 : value;
  const uint8_t codes[2] = {static_cast<uint8_t>('0' + clamped / 10),
                            static_cast<uint8_t>('0' + clamped % 10)};
  float widths[2] = {};
  float boxW = 0.0f;
  float total = 0.0f;
  const float boxH = static_cast<float>(DIGIT_HEIGHT);
  fitLine(codes, 2, boxH, static_cast<float>(COLUMNS) - EDGE_PAD, 0.0f, true, &boxW, widths, &total);
  const float left = (static_cast<float>(COLUMNS) - total) * 0.5f;
  stampFitted(canvas, codes, 2, left, DIGIT_TOP, boxW, boxH, widths, 0, colour);
}

}  // namespace

void drawSmoothLine(const RenderRequest& request, Core::DisplayContent content, uint16_t destX,
                    uint16_t destY, uint16_t destW, uint16_t destH, uint16_t columns, uint16_t rows,
                    uint32_t* pixels) {
  if (pixels == nullptr || destW == 0 || destH == 0) return;

  Canvas canvas;
  canvas.pixels = pixels;
  canvas.destX = destX;
  canvas.destY = destY;
  canvas.destW = destW;
  canvas.destH = destH;
  canvas.columns = columns;
  canvas.rows = rows;
  canvas.sx = static_cast<float>(destW) / static_cast<float>(COLUMNS);
  canvas.sy = static_cast<float>(destH) / static_cast<float>(ROWS);

  const uint32_t colour = colourFor(request.light);
  const Core::DisplayContent shown = contentAvailable(content) ? content : Core::DisplayContent::Clock;

  switch (shown) {
    case Core::DisplayContent::Blank:
      break;
    case Core::DisplayContent::ClockAndEnd:
      if (request.showEndLabels) {
        drawEndLabel(canvas, request, colour);
      } else {
        drawPair(canvas, static_cast<uint8_t>(request.endNumber > 99 ? 99 : request.endNumber), colour);
      }
      break;
    case Core::DisplayContent::ArrowCount:
      drawDigit(canvas, request.arrowsShot > 9 ? 9 : request.arrowsShot, 7.0f, DIGIT_TOP, false, colour);
      drawSeparator(canvas, colour);
      drawDigit(canvas, request.arrowsPerEnd > 9 ? 9 : request.arrowsPerEnd, 20.0f, DIGIT_TOP, false,
                colour);
      break;
    case Core::DisplayContent::Score:
      drawTwoPairs(canvas, request.score[0], request.score[1], colour);
      break;
    case Core::DisplayContent::SetPoints:
      drawTwoPairs(canvas, request.setPoints[0], request.setPoints[1], colour);
      break;
    case Core::DisplayContent::Shooter:
      drawPair(canvas, request.shooter, colour);
      break;
    case Core::DisplayContent::Clock:
    default:
      drawClock(canvas, request, colour);
      break;
  }
}

}  // namespace DisplayLogic
