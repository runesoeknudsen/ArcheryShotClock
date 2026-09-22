#include "smooth_draw.h"

#include <math.h>

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

enum class Prim : uint8_t { Capsule, Ellipse, Arc };

struct Stroke {
  Prim kind = Prim::Capsule;
  float x0 = 0;
  float y0 = 0;
  float x1 = 0;
  float y1 = 0;
  float a0 = 0;
  float a1 = 0;
  float radius = 0;
};

struct StrokeBuf {
  Stroke items[24];
  uint8_t count = 0;

  void add(float x0, float y0, float x1, float y1, float radius) {
    if (count >= 24) return;
    Stroke stroke;
    stroke.kind = Prim::Capsule;
    stroke.x0 = x0;
    stroke.y0 = y0;
    stroke.x1 = x1;
    stroke.y1 = y1;
    stroke.radius = radius;
    items[count++] = stroke;
  }

  void ellipse(float cx, float cy, float rx, float ry, float radius) {
    if (count >= 24) return;
    Stroke stroke;
    stroke.kind = Prim::Ellipse;
    stroke.x0 = cx;
    stroke.y0 = cy;
    stroke.x1 = rx;
    stroke.y1 = ry;
    stroke.radius = radius;
    items[count++] = stroke;
  }

  void arc(float cx, float cy, float rx, float ry, float a0, float a1, float radius) {
    if (count >= 24) return;
    Stroke stroke;
    stroke.kind = Prim::Arc;
    stroke.x0 = cx;
    stroke.y0 = cy;
    stroke.x1 = rx;
    stroke.y1 = ry;
    stroke.a0 = a0;
    stroke.a1 = a1;
    stroke.radius = radius;
    items[count++] = stroke;
  }
};

constexpr float kTwoPi = 6.2831853f;

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

bool angleOnArc(float ang, float a0, float a1) {
  float t = ang - a0;
  while (t < 0.0f) t += kTwoPi;
  while (t >= kTwoPi) t -= kTwoPi;
  return t <= a1 - a0;
}

void ellipsePoint(float cx, float cy, float rx, float ry, float ang, float& x, float& y) {
  x = cx + rx * cosf(ang);
  y = cy + ry * sinf(ang);
}

float distToEllipse(float px, float py, float cx, float cy, float rx, float ry) {
  if (rx < 0.01f) rx = 0.01f;
  if (ry < 0.01f) ry = 0.01f;
  const float u = (px - cx) / rx;
  const float v = (py - cy) / ry;
  const float len = sqrtf(u * u + v * v);
  if (len < 1e-6f) return -(rx < ry ? rx : ry);
  const float ex = cx + (u / len) * rx;
  const float ey = cy + (v / len) * ry;
  const float dx = px - ex;
  const float dy = py - ey;
  float dist = sqrtf(dx * dx + dy * dy);
  if (len < 1.0f) dist = -dist;
  return dist;
}

float distToArc(float px, float py, float cx, float cy, float rx, float ry, float a0, float a1) {
  if (rx < 0.01f) rx = 0.01f;
  if (ry < 0.01f) ry = 0.01f;
  const float u = (px - cx) / rx;
  const float v = (py - cy) / ry;
  const float ang = atan2f(v, u);
  if (angleOnArc(ang, a0, a1)) return fabsf(distToEllipse(px, py, cx, cy, rx, ry));
  float x0 = 0;
  float y0 = 0;
  float x1 = 0;
  float y1 = 0;
  ellipsePoint(cx, cy, rx, ry, a0, x0, y0);
  ellipsePoint(cx, cy, rx, ry, a1, x1, y1);
  const float d0 = distToSeg(px, py, x0, y0, x0, y0);
  const float d1 = distToSeg(px, py, x1, y1, x1, y1);
  return d0 < d1 ? d0 : d1;
}

float primSdf(const Stroke& stroke, float px, float py) {
  if (stroke.kind == Prim::Ellipse) {
    return fabsf(distToEllipse(px, py, stroke.x0, stroke.y0, stroke.x1, stroke.y1)) - stroke.radius;
  }
  if (stroke.kind == Prim::Arc) {
    return distToArc(px, py, stroke.x0, stroke.y0, stroke.x1, stroke.y1, stroke.a0, stroke.a1) -
           stroke.radius;
  }
  return distToSeg(px, py, stroke.x0, stroke.y0, stroke.x1, stroke.y1) - stroke.radius;
}

void primBounds(const Stroke& stroke, float pad, float& minX, float& minY, float& maxX, float& maxY) {
  if (stroke.kind == Prim::Ellipse || stroke.kind == Prim::Arc) {
    const float x0 = stroke.x0 - stroke.x1 - pad;
    const float y0 = stroke.y0 - stroke.y1 - pad;
    const float x1 = stroke.x0 + stroke.x1 + pad;
    const float y1 = stroke.y0 + stroke.y1 + pad;
    if (x0 < minX) minX = x0;
    if (y0 < minY) minY = y0;
    if (x1 > maxX) maxX = x1;
    if (y1 > maxY) maxY = y1;
    return;
  }
  const float x0 = (stroke.x0 < stroke.x1 ? stroke.x0 : stroke.x1) - pad;
  const float x1 = (stroke.x0 > stroke.x1 ? stroke.x0 : stroke.x1) + pad;
  const float y0 = (stroke.y0 < stroke.y1 ? stroke.y0 : stroke.y1) - pad;
  const float y1 = (stroke.y0 > stroke.y1 ? stroke.y0 : stroke.y1) + pad;
  if (x0 < minX) minX = x0;
  if (y0 < minY) minY = y0;
  if (x1 > maxX) maxX = x1;
  if (y1 > maxY) maxY = y1;
}

float strokeRadius(const Canvas& canvas, float unitHalf) {
  float radius = unitHalf * 0.5f * (canvas.sx + canvas.sy);
  if (radius < 0.55f) radius = 0.55f;
  return radius;
}

void unitToDest(const Canvas& canvas, float ux, float uy, float& dx, float& dy) {
  dx = static_cast<float>(canvas.destX) + ux * canvas.sx;
  dy = static_cast<float>(canvas.destY) + uy * canvas.sy;
}

void addUnitStroke(const Canvas& canvas, StrokeBuf& buf, float ux0, float uy0, float ux1, float uy1,
                   float radius) {
  float x0 = 0;
  float y0 = 0;
  float x1 = 0;
  float y1 = 0;
  unitToDest(canvas, ux0, uy0, x0, y0);
  unitToDest(canvas, ux1, uy1, x1, y1);
  buf.add(x0, y0, x1, y1, radius);
}

void addUnitEllipse(const Canvas& canvas, StrokeBuf& buf, float ucx, float ucy, float urx, float ury,
                    float radius) {
  float cx = 0;
  float cy = 0;
  unitToDest(canvas, ucx, ucy, cx, cy);
  buf.ellipse(cx, cy, urx * canvas.sx, ury * canvas.sy, radius);
}

void addUnitArc(const Canvas& canvas, StrokeBuf& buf, float ucx, float ucy, float urx, float ury,
                float a0, float a1, float radius) {
  float cx = 0;
  float cy = 0;
  unitToDest(canvas, ucx, ucy, cx, cy);
  buf.arc(cx, cy, urx * canvas.sx, ury * canvas.sy, a0, a1, radius);
}

void stampStrokes(const Canvas& canvas, const StrokeBuf& buf, uint32_t colour) {
  if (buf.count == 0 || canvas.pixels == nullptr || canvas.destW == 0 || canvas.destH == 0) return;

  float minX = 1e9f;
  float minY = 1e9f;
  float maxX = -1e9f;
  float maxY = -1e9f;
  for (uint8_t index = 0; index < buf.count; index++) {
    primBounds(buf.items[index], buf.items[index].radius + 2.2f, minX, minY, maxX, maxY);
  }

  int x0 = static_cast<int>(minX);
  int y0 = static_cast<int>(minY);
  int x1 = static_cast<int>(maxX + 0.999f);
  int y1 = static_cast<int>(maxY + 0.999f);
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

  // Wide enough that an axis-aligned edge always lights a dimmer LED on
  // each side. A half-pixel band falls between LED centres and reads as a
  // staircase again.
  float aa = 0.50f * (canvas.sx < canvas.sy ? canvas.sx : canvas.sy);
  if (aa < 0.90f) aa = 0.90f;
  if (aa > 1.70f) aa = 1.70f;
  for (int py = y0; py < y1; py++) {
    for (int px = x0; px < x1; px++) {
      const float cx = static_cast<float>(px) + 0.5f;
      const float cy = static_cast<float>(py) + 0.5f;
      float sdf = 1e9f;
      for (uint8_t index = 0; index < buf.count; index++) {
        const float dist = primSdf(buf.items[index], cx, cy);
        if (dist < sdf) sdf = dist;
      }
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

// Grotesque sans after Source Sans / Helvetica / DIN signage. One cell,
// one stroke weight, and the same bowls and stems for digits and letters
// so the clock reads as a single face.
constexpr float kPi = 3.14159265f;

struct Face {
  float xL = 0;
  float xR = 0;
  float xM = 0;
  float yT = 0;
  float yB = 0;
  float yM = 0;
  float w = 0;
  float h = 0;
};

Face makeFace(float left, float top, float width, float height) {
  Face face;
  face.w = width;
  face.h = height;
  const float insetX = width * 0.14f;
  const float insetY = height <= 6.0f ? height * 0.12f : height * 0.08f;
  face.xL = left + insetX;
  face.xR = left + width - insetX;
  face.xM = left + width * 0.50f;
  face.yT = top + insetY;
  face.yB = top + height - insetY;
  face.yM = 0.5f * (face.yT + face.yB);
  return face;
}

void addDigit(const Canvas& canvas, StrokeBuf& buf, uint8_t value, float left, float top, float width,
              float height, float radius) {
  if (value > 9) return;
  const Face f = makeFace(left, top, width, height);
  const float rx = width * 0.36f;
  const float ry = height * 0.42f;
  const float bowlRy = height * (height <= 6.0f ? 0.24f : 0.22f);

  switch (value) {
    case 0:
      addUnitEllipse(canvas, buf, f.xM, f.yM, rx, ry, radius);
      return;
    case 1: {
      const float stemX = left + width * 0.56f;
      addUnitStroke(canvas, buf, left + width * 0.20f, f.yT + height * 0.16f, stemX, f.yT, radius);
      addUnitStroke(canvas, buf, stemX, f.yT, stemX, f.yB, radius);
      addUnitStroke(canvas, buf, left + width * 0.18f, f.yB, left + width * 0.84f, f.yB, radius);
      return;
    }
    case 2: {
      const float topRy = height <= 6.0f ? height * 0.22f : height * 0.20f;
      addUnitArc(canvas, buf, f.xM, f.yT + height * 0.22f, rx, topRy, kPi * 0.80f, kPi * 2.15f,
                 radius);
      addUnitStroke(canvas, buf, f.xR, f.yT + height * 0.36f, f.xL + width * 0.02f, f.yB, radius);
      addUnitStroke(canvas, buf, f.xL, f.yB, f.xR, f.yB, radius);
      return;
    }
    case 3: {
      const float topCy = f.yT + height * (height <= 6.0f ? 0.20f : 0.25f);
      const float botCy = f.yB - height * (height <= 6.0f ? 0.20f : 0.25f);
      const float rBowl = height <= 6.0f ? height * 0.18f : bowlRy;
      addUnitArc(canvas, buf, f.xM + width * 0.02f, topCy, rx, rBowl, -kPi * 0.62f, kPi * 0.78f,
                 radius);
      addUnitArc(canvas, buf, f.xM + width * 0.02f, botCy, rx, rBowl, -kPi * 0.78f, kPi * 0.62f,
                 radius);
      return;
    }
    case 4:
      addUnitStroke(canvas, buf, f.xR - width * 0.06f, f.yT, f.xR - width * 0.06f, f.yB, radius);
      addUnitStroke(canvas, buf, f.xR - width * 0.06f, f.yT, f.xL, f.yT + height * 0.56f, radius);
      addUnitStroke(canvas, buf, f.xL, f.yT + height * 0.56f, f.xR, f.yT + height * 0.56f, radius);
      return;
    case 5:
      addUnitStroke(canvas, buf, f.xR, f.yT, f.xL, f.yT, radius);
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yM, radius);
      addUnitArc(canvas, buf, f.xM, f.yB - height * 0.26f, rx, bowlRy, -kPi * 0.82f, kPi * 0.70f,
                 radius);
      return;
    case 6:
      addUnitEllipse(canvas, buf, f.xM, f.yB - height * 0.26f, rx, height * 0.24f, radius);
      addUnitArc(canvas, buf, f.xM, f.yM - height * 0.04f, rx, height * 0.38f, kPi * 0.52f,
                 kPi * 1.88f, radius);
      return;
    case 7:
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xR, f.yT, radius);
      addUnitStroke(canvas, buf, f.xR, f.yT, f.xL + width * 0.16f, f.yB, radius);
      return;
    case 8:
      addUnitEllipse(canvas, buf, f.xM, f.yT + height * 0.24f, width * 0.33f, height * 0.20f, radius);
      addUnitEllipse(canvas, buf, f.xM, f.yB - height * 0.24f, width * 0.36f, height * 0.22f, radius);
      return;
    case 9:
      addUnitEllipse(canvas, buf, f.xM, f.yT + height * 0.28f, rx * 0.95f, height * 0.22f, radius);
      addUnitArc(canvas, buf, f.xM, f.yM + height * 0.06f, rx, height * 0.36f, -kPi * 0.08f,
                 kPi * 0.82f, radius);
      return;
    default:
      return;
  }
}

void drawDigit(const Canvas& canvas, uint8_t value, float left, float top, bool compact, uint32_t colour) {
  StrokeBuf buf;
  const float width = DIGIT_WIDTH;
  const float height = compact ? static_cast<float>(SmallFont::DIGIT_HEIGHT) : DIGIT_HEIGHT;
  const float radius = strokeRadius(canvas, compact ? 0.38f : 0.46f);
  addDigit(canvas, buf, value, left, top, width, height, radius);
  stampStrokes(canvas, buf, colour);
}

void drawColon(const Canvas& canvas, float origin, float top, bool compact, uint32_t colour) {
  StrokeBuf buf;
  const float radius = strokeRadius(canvas, compact ? 0.55f : 0.70f);
  const float cx = origin + 13.5f;
  if (compact) {
    addUnitStroke(canvas, buf, cx, top + 1.5f, cx, top + 1.5f, radius);
    addUnitStroke(canvas, buf, cx, top + 3.5f, cx, top + 3.5f, radius);
  } else {
    addUnitStroke(canvas, buf, cx, top + 3.5f, cx, top + 3.5f, radius);
    addUnitStroke(canvas, buf, cx, top + 7.5f, cx, top + 7.5f, radius);
  }
  stampStrokes(canvas, buf, colour);
}

void drawSeparator(const Canvas& canvas, uint32_t colour) {
  StrokeBuf buf;
  const float radius = strokeRadius(canvas, 0.70f);
  addUnitStroke(canvas, buf, 14.4f, 8.0f, 17.6f, 8.0f, radius);
  stampStrokes(canvas, buf, colour);
}

void addSansLetter(const Canvas& canvas, StrokeBuf& buf, char letter, float left, float top,
                   float width, float height, float radius) {
  const Face f = makeFace(left, top, width, height);
  const float rx = width * 0.36f;
  const float ry = height * 0.40f;
  const float bowlRx = f.xR - f.xL;
  const float upperRy = height * 0.20f;
  const float lowerRy = height * 0.22f;

  switch (letter) {
    case 'A':
      addUnitStroke(canvas, buf, f.xL, f.yB, f.xM, f.yT, radius);
      addUnitStroke(canvas, buf, f.xR, f.yB, f.xM, f.yT, radius);
      addUnitStroke(canvas, buf, f.xL + width * 0.10f, f.yT + height * 0.58f,
                    f.xR - width * 0.10f, f.yT + height * 0.58f, radius);
      return;
    case 'B':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitArc(canvas, buf, f.xL, f.yT + height * 0.22f, bowlRx, upperRy, -kPi * 0.50f,
                 kPi * 0.50f, radius);
      addUnitArc(canvas, buf, f.xL, f.yB - height * 0.24f, bowlRx, lowerRy, -kPi * 0.50f,
                 kPi * 0.50f, radius);
      return;
    case 'C':
      addUnitArc(canvas, buf, f.xM, f.yM, rx, ry, kPi * 0.32f, kPi * 1.68f, radius);
      return;
    case 'D':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitArc(canvas, buf, f.xL, f.yM, bowlRx, ry, -kPi * 0.50f, kPi * 0.50f, radius);
      return;
    case 'E':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xR, f.yT, radius);
      addUnitStroke(canvas, buf, f.xL, f.yM, f.xL + width * 0.58f, f.yM, radius);
      addUnitStroke(canvas, buf, f.xL, f.yB, f.xR, f.yB, radius);
      return;
    case 'F':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xR, f.yT, radius);
      addUnitStroke(canvas, buf, f.xL, f.yM, f.xL + width * 0.58f, f.yM, radius);
      return;
    case 'G':
      addUnitArc(canvas, buf, f.xM, f.yM, rx, ry, kPi * 0.20f, kPi * 1.78f, radius);
      addUnitStroke(canvas, buf, f.xM + width * 0.18f, f.yM, f.xR, f.yM, radius);
      addUnitStroke(canvas, buf, f.xR, f.yM, f.xR, f.yM + height * 0.28f, radius);
      return;
    case 'H':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitStroke(canvas, buf, f.xR, f.yT, f.xR, f.yB, radius);
      addUnitStroke(canvas, buf, f.xL, f.yM, f.xR, f.yM, radius);
      return;
    case 'K':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitStroke(canvas, buf, f.xR, f.yT, f.xL, f.yM, radius);
      addUnitStroke(canvas, buf, f.xL, f.yM, f.xR, f.yB, radius);
      return;
    case 'N':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitStroke(canvas, buf, f.xR, f.yT, f.xR, f.yB, radius);
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xR, f.yB, radius);
      return;
    case 'O':
      addUnitEllipse(canvas, buf, f.xM, f.yM, rx, ry, radius);
      return;
    case 'R':
      addUnitStroke(canvas, buf, f.xL, f.yT, f.xL, f.yB, radius);
      addUnitArc(canvas, buf, f.xL, f.yT + height * 0.22f, bowlRx, upperRy, -kPi * 0.50f,
                 kPi * 0.50f, radius);
      addUnitStroke(canvas, buf, f.xL + width * 0.08f, f.yM, f.xR, f.yB, radius);
      return;
    case 'S':
      addUnitArc(canvas, buf, f.xM, f.yT + height * 0.22f, rx, height * 0.22f, kPi * 1.00f,
                 kPi * 1.92f, radius);
      addUnitStroke(canvas, buf, f.xL, f.yT + height * 0.22f, f.xR, f.yB - height * 0.22f, radius);
      addUnitArc(canvas, buf, f.xM, f.yB - height * 0.22f, rx, height * 0.22f, kPi * 0.08f, kPi,
                 radius);
      return;
    default:
      return;
  }
}

void addWideLetter(const Canvas& canvas, StrokeBuf& buf, char letter, float left, float top,
                   float radius) {
  addSansLetter(canvas, buf, letter, left, top, static_cast<float>(SmallFont::WIDE_WIDTH),
                static_cast<float>(SmallFont::WIDE_HEIGHT), radius);
}

void addNarrowLetter(const Canvas& canvas, StrokeBuf& buf, char letter, float left, float top,
                     float radius) {
  addSansLetter(canvas, buf, letter, left, top, static_cast<float>(SmallFont::NARROW_WIDTH),
                static_cast<float>(SmallFont::NARROW_HEIGHT), radius);
}

void drawWideWord(const Canvas& canvas, const char* word, float top, uint32_t colour) {
  uint8_t count = 0;
  while (word[count] != '\0') count++;
  if (count == 0) return;
  const float width = static_cast<float>(count * SmallFont::WIDE_WIDTH + (count - 1) * LETTER_GAP);
  float left = width >= COLUMNS ? 0.0f : (static_cast<float>(COLUMNS) - width) * 0.5f;
  const float radius = strokeRadius(canvas, 0.36f);
  for (uint8_t index = 0; index < count; index++) {
    StrokeBuf buf;
    addWideLetter(canvas, buf, word[index], left, top, radius);
    stampStrokes(canvas, buf, colour);
    left += SmallFont::WIDE_WIDTH + LETTER_GAP;
  }
}

void drawNarrowLetter(const Canvas& canvas, char letter, float left, float top, uint32_t colour) {
  StrokeBuf buf;
  addNarrowLetter(canvas, buf, letter, left, top, strokeRadius(canvas, 0.34f));
  stampStrokes(canvas, buf, colour);
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
