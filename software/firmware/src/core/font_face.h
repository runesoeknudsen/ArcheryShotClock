#pragma once

#include <cstdint>

// One exported typeface. software/tools/font_to_glyphs.py writes a generated
// header of these tables from a TTF/OTF. The large-panel renderer stamps the
// filled outlines at the cabinet resolution. Swap activeFontFace() to try
// another generated face; the 32x16 bitmap path never reads this.

namespace DisplayLogic {

struct GlyphPoint {
  int16_t x = 0;
  int16_t y = 0;

  // The ESP32 toolchain compiles as C++11, where a default member initializer
  // stops a struct being an aggregate. The generated font tables use brace
  // lists, so these constructors are what that list initialization calls.
  GlyphPoint() = default;
  GlyphPoint(int16_t x, int16_t y) : x(x), y(y) {}
};

struct GlyphContour {
  uint16_t first = 0;
  uint16_t count = 0;

  GlyphContour() = default;
  GlyphContour(uint16_t first, uint16_t count) : first(first), count(count) {}
};

struct FontGlyph {
  uint8_t code = 0;
  uint16_t contour0 = 0;
  uint8_t contours = 0;
  int16_t minX = 0;
  int16_t minY = 0;
  int16_t maxX = 0;
  int16_t maxY = 0;

  FontGlyph() = default;
  FontGlyph(uint8_t code, uint16_t contour0, uint8_t contours, int16_t minX, int16_t minY, int16_t maxX,
            int16_t maxY)
      : code(code),
        contour0(contour0),
        contours(contours),
        minX(minX),
        minY(minY),
        maxX(maxX),
        maxY(maxY) {}
};

struct FontFace {
  const char* name = nullptr;
  const char* sourceFont = nullptr;
  uint16_t unitsPerEm = 1000;
  uint16_t capHeight = 700;
  uint16_t maxInkWidth = 700;
  const GlyphPoint* points = nullptr;
  uint16_t pointCount = 0;
  const GlyphContour* contours = nullptr;
  uint16_t contourCount = 0;
  const FontGlyph* glyphs = nullptr;
  uint8_t glyphCount = 0;

  FontFace() = default;
  FontFace(const char* name, const char* sourceFont, uint16_t unitsPerEm, uint16_t capHeight,
           uint16_t maxInkWidth, const GlyphPoint* points, uint16_t pointCount, const GlyphContour* contours,
           uint16_t contourCount, const FontGlyph* glyphs, uint8_t glyphCount)
      : name(name),
        sourceFont(sourceFont),
        unitsPerEm(unitsPerEm),
        capHeight(capHeight),
        maxInkWidth(maxInkWidth),
        points(points),
        pointCount(pointCount),
        contours(contours),
        contourCount(contourCount),
        glyphs(glyphs),
        glyphCount(glyphCount) {}
};

const FontFace& activeFontFace();

inline const FontGlyph* findGlyph(const FontFace& face, uint8_t code) {
  for (uint8_t index = 0; index < face.glyphCount; index++) {
    if (face.glyphs[index].code == code) return &face.glyphs[index];
  }
  return nullptr;
}

}  // namespace DisplayLogic
