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
};

struct GlyphContour {
  uint16_t first = 0;
  uint16_t count = 0;
};

struct FontGlyph {
  uint8_t code = 0;
  uint16_t contour0 = 0;
  uint8_t contours = 0;
  int16_t minX = 0;
  int16_t minY = 0;
  int16_t maxX = 0;
  int16_t maxY = 0;
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
};

const FontFace& activeFontFace();

inline const FontGlyph* findGlyph(const FontFace& face, uint8_t code) {
  for (uint8_t index = 0; index < face.glyphCount; index++) {
    if (face.glyphs[index].code == code) return &face.glyphs[index];
  }
  return nullptr;
}

}  // namespace DisplayLogic
