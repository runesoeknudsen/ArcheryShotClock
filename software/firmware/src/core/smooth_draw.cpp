#include "smooth_draw.h"

#include <math.h>

#include "font_face.h"
#include "small_font.h"

namespace DisplayLogic {
namespace {

constexpr uint8_t DIGIT_WIDTH = 5;
constexpr uint8_t DIGIT_HEIGHT = 11;
constexpr uint8_t DIGIT_GAP = 2;
constexpr uint8_t DIGIT_TOP = (ROWS - DIGIT_HEIGHT) / 2;
constexpr uint8_t GROUP_LEFT = 0;
constexpr uint8_t LETTER_GAP = 1;
constexpr uint8_t ELEMENT_GAP = 1;

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

void drawWideWord(const Canvas& canvas, const char* word, float top, uint32_t colour) {
  uint8_t count = 0;
  while (word[count] != '\0') count++;
  if (count == 0) return;
  const float width = static_cast<float>(count * SmallFont::WIDE_WIDTH + (count - 1) * LETTER_GAP);
  float left = width >= COLUMNS ? 0.0f : (static_cast<float>(COLUMNS) - width) * 0.5f;
  for (uint8_t index = 0; index < count; index++) {
    stampGlyph(canvas, static_cast<uint8_t>(word[index]), left, top, SmallFont::WIDE_WIDTH,
               SmallFont::WIDE_HEIGHT, colour);
    left += SmallFont::WIDE_WIDTH + LETTER_GAP;
  }
}

void drawNarrowLetter(const Canvas& canvas, char letter, float left, float top, uint32_t colour) {
  stampGlyph(canvas, static_cast<uint8_t>(letter), left, top, SmallFont::NARROW_WIDTH,
             SmallFont::NARROW_HEIGHT, colour);
}

uint8_t minTimeLeft(bool groupVertical) {
  if (!groupVertical) return 0;
  return static_cast<uint8_t>(GROUP_LEFT + SmallFont::NARROW_WIDTH + ELEMENT_GAP);
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

void drawMmSs(const Canvas& canvas, uint32_t totalSeconds, uint32_t colour, float top, float origin,
              bool compact) {
  const uint32_t minutes = (totalSeconds / 60) % 100;
  const uint32_t seconds = totalSeconds % 60;
  drawDigit(canvas, static_cast<uint8_t>(minutes / 10), origin, top, compact, colour);
  drawDigit(canvas, static_cast<uint8_t>(minutes % 10), origin + DIGIT_WIDTH + DIGIT_GAP, top, compact,
            colour);
  drawDigit(canvas, static_cast<uint8_t>(seconds / 10), origin + 16.0f, top, compact, colour);
  drawDigit(canvas, static_cast<uint8_t>(seconds % 10), origin + 23.0f, top, compact, colour);
  drawColon(canvas, origin, top, compact, colour);
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

void drawRightSeconds(const Canvas& canvas, uint32_t totalSeconds, float top, uint32_t colour,
                      float minLeft, bool compact) {
  uint32_t shown = totalSeconds > 9999 ? 9999 : totalSeconds;
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
  drawWideWord(canvas, scoring ? "SCORE" : "END", 0.0f, colour);
  const uint16_t shown = request.endNumber > 99 ? 99 : request.endNumber;
  const float top = static_cast<float>(SmallFont::WIDE_HEIGHT + ELEMENT_GAP);
  drawDigit(canvas, static_cast<uint8_t>(shown / 10), 10.0f, top, true, colour);
  drawDigit(canvas, static_cast<uint8_t>(shown % 10), 17.0f, top, true, colour);
}

void drawGroup(const Canvas& canvas, const char* letters, uint32_t colour, bool vertical) {
  if (vertical) {
    drawNarrowLetter(canvas, letters[0], GROUP_LEFT, 2.0f, colour);
    drawNarrowLetter(canvas, letters[1], GROUP_LEFT, 9.0f, colour);
    return;
  }
  const float width = static_cast<float>(SmallFont::NARROW_WIDTH * 2 + LETTER_GAP);
  const float left = static_cast<float>(CLOCK_ONES_LEFT + DIGIT_WIDTH) - width;
  drawNarrowLetter(canvas, letters[0], left, 11.0f, colour);
  drawNarrowLetter(canvas, letters[1], left + SmallFont::NARROW_WIDTH + LETTER_GAP, 11.0f, colour);
}

void drawClock(const Canvas& canvas, const RenderRequest& request, uint32_t colour) {
  if (request.showEndLabels && endLabelPhase(request.phase)) {
    drawEndLabel(canvas, request, colour);
    return;
  }

  const uint32_t totalSeconds = (request.remainingMs + 999) / 1000;
  const bool group = showGroup(request);
  const bool groupVertical = group && request.abcdVertical;
  const bool groupUnder = group && !request.abcdVertical;
  const char* letters = groupLetters(request.detail);
  const bool compactTime = request.phase == Core::Phase::Break || groupUnder;
  const float top = compactTime ? 0.0f : DIGIT_TOP;
  const float origin = groupVertical ? minTimeLeft(true) : 2.0f;

  if (request.phase == Core::Phase::Break) {
    drawWideWord(canvas, "BREAK", 0.0f, colour);
    const float timeTop = static_cast<float>(SmallFont::WIDE_HEIGHT + ELEMENT_GAP);
    drawMmSs(canvas, totalSeconds, colour, timeTop, 2.0f, true);
    return;
  }

  if (request.clockSeconds) {
    drawRightSeconds(canvas, totalSeconds, top, colour, static_cast<float>(minTimeLeft(groupVertical)),
                     compactTime);
  } else {
    drawMmSs(canvas, totalSeconds, colour, top, origin, compactTime);
  }

  if (!group) return;
  drawGroup(canvas, letters, groupColour(request, colour), groupVertical);
}

void drawTwoPairs(const Canvas& canvas, uint16_t left, uint16_t right, uint32_t colour) {
  const uint8_t a = left > 99 ? 99 : static_cast<uint8_t>(left);
  const uint8_t b = right > 99 ? 99 : static_cast<uint8_t>(right);
  drawDigit(canvas, a / 10, 2.0f, DIGIT_TOP, false, colour);
  drawDigit(canvas, a % 10, 9.0f, DIGIT_TOP, false, colour);
  drawDigit(canvas, b / 10, 18.0f, DIGIT_TOP, false, colour);
  drawDigit(canvas, b % 10, 25.0f, DIGIT_TOP, false, colour);
}

void drawPair(const Canvas& canvas, uint8_t value, uint32_t colour) {
  const uint8_t clamped = value > 99 ? 99 : value;
  drawDigit(canvas, clamped / 10, 10.0f, DIGIT_TOP, false, colour);
  drawDigit(canvas, clamped % 10, 17.0f, DIGIT_TOP, false, colour);
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
