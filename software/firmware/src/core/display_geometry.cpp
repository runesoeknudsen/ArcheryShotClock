#include "display_geometry.h"

#include "display_logic.h"

namespace DisplayLogic {
namespace {

bool sameText(const char* left, const char* right) {
  if (left == nullptr || right == nullptr) return false;
  uint8_t index = 0;
  while (left[index] != '\0' && right[index] != '\0') {
    if (left[index] != right[index]) return false;
    index++;
  }
  return left[index] == right[index];
}

uint8_t stackedP5(PanelPreset preset) {
  switch (preset) {
    case PanelPreset::P5_64x32: return 1;
    case PanelPreset::P5_64x64: return 2;
    case PanelPreset::P5_96x64: return 3;
    case PanelPreset::P5_128x64: return 4;
    case PanelPreset::P5_160x64: return 5;
    case PanelPreset::Led32x16:
    default: return 0;
  }
}

void landscapeSize(PanelPreset preset, uint16_t& columns, uint16_t& rows) {
  const uint8_t count = stackedP5(preset);
  if (count == 0) {
    columns = TILE_COLUMNS;
    rows = TILE_ROWS;
    return;
  }
  if (count == 1) {
    columns = 64;
    rows = 32;
    return;
  }
  columns = static_cast<uint16_t>(count * 32);
  rows = 64;
}

const Core::DisplayContent kDefaultOrder[] = {
    Core::DisplayContent::Clock,      Core::DisplayContent::ClockAndEnd, Core::DisplayContent::Score,
    Core::DisplayContent::ArrowCount, Core::DisplayContent::SetPoints,   Core::DisplayContent::Shooter};

bool mapLandscapeToHub75(PanelPreset preset, uint16_t x, uint16_t y, uint16_t& dmaX, uint16_t& dmaY) {
  uint16_t columns = 0;
  uint16_t rows = 0;
  landscapeSize(preset, columns, rows);
  if (x >= columns || y >= rows) return false;

  const uint8_t count = stackedP5(preset);
  if (count <= 1) {
    dmaX = x;
    dmaY = y;
    return true;
  }

  // Two to five 64x32 modules stood on end (90° CW) and placed left to right.
  // DMA is one 64*N x 32 chain; each 32x64 logical strip is one module.
  const uint16_t panel = static_cast<uint16_t>(x / 32);
  const uint16_t localX = static_cast<uint16_t>(x % 32);
  dmaX = static_cast<uint16_t>(panel * 64 + y);
  dmaY = static_cast<uint16_t>(31 - localX);
  return true;
}

}  // namespace

Geometry geometryFor(PanelPreset preset, Orientation orientation) {
  Geometry geometry;
  geometry.preset = preset;
  geometry.orientation = orientation;
  landscapeSize(preset, geometry.columns, geometry.rows);
  if (preset != PanelPreset::Led32x16 && orientation == Orientation::Portrait) {
    const uint16_t columns = geometry.columns;
    geometry.columns = geometry.rows;
    geometry.rows = columns;
  } else {
    geometry.orientation = Orientation::Landscape;
  }
  return geometry;
}

uint16_t pixelCount(const Geometry& geometry) {
  const uint32_t count = static_cast<uint32_t>(geometry.columns) * geometry.rows;
  return count > MAX_PIXEL_COUNT ? MAX_PIXEL_COUNT : static_cast<uint16_t>(count);
}

uint8_t p5Count(PanelPreset preset) { return stackedP5(preset); }

uint8_t maxLinesFor(const Geometry& geometry) {
  uint8_t lines = static_cast<uint8_t>(geometry.rows / 8);
  if (lines < 1) lines = 1;
  if (lines > MAX_CONTENT_LINES) lines = MAX_CONTENT_LINES;
  return lines;
}

uint8_t defaultLineCount(const Geometry& geometry) {
  if (geometry.orientation == Orientation::Portrait) return maxLinesFor(geometry);
  return 1;
}

uint8_t defaultLines(const Geometry& geometry, Core::DisplayContent* lines, uint8_t maxLines) {
  uint8_t count = defaultLineCount(geometry);
  constexpr uint8_t known = sizeof(kDefaultOrder) / sizeof(kDefaultOrder[0]);
  if (count > known) count = known;
  if (count > maxLines) count = maxLines;
  for (uint8_t index = 0; index < count; index++) {
    lines[index] = kDefaultOrder[index];
  }
  return count;
}

bool validPreset(uint8_t value) { return value <= static_cast<uint8_t>(PanelPreset::P5_160x64); }

bool validOrientation(uint8_t value) { return value <= static_cast<uint8_t>(Orientation::Portrait); }

bool validLineScale(uint8_t value) { return value <= static_cast<uint8_t>(LineScaleMode::Hero); }

const char* name(PanelPreset preset) {
  switch (preset) {
    case PanelPreset::Led32x16: return "LED_32X16";
    case PanelPreset::P5_64x32: return "P5_64X32";
    case PanelPreset::P5_64x64: return "P5_64X64";
    case PanelPreset::P5_96x64: return "P5_96X64";
    case PanelPreset::P5_128x64: return "P5_128X64";
    case PanelPreset::P5_160x64: return "P5_160X64";
  }
  return "UNKNOWN";
}

const char* name(Orientation orientation) {
  switch (orientation) {
    case Orientation::Landscape: return "LANDSCAPE";
    case Orientation::Portrait: return "PORTRAIT";
  }
  return "UNKNOWN";
}

const char* name(LineScaleMode mode) {
  switch (mode) {
    case LineScaleMode::Fill: return "FILL";
    case LineScaleMode::Hero: return "HERO";
  }
  return "UNKNOWN";
}

bool parsePreset(const char* text, PanelPreset& out) {
  if (sameText(text, "LED_32X16")) {
    out = PanelPreset::Led32x16;
    return true;
  }
  if (sameText(text, "P5_64X32")) {
    out = PanelPreset::P5_64x32;
    return true;
  }
  if (sameText(text, "P5_64X64")) {
    out = PanelPreset::P5_64x64;
    return true;
  }
  if (sameText(text, "P5_96X64")) {
    out = PanelPreset::P5_96x64;
    return true;
  }
  if (sameText(text, "P5_128X64")) {
    out = PanelPreset::P5_128x64;
    return true;
  }
  if (sameText(text, "P5_160X64")) {
    out = PanelPreset::P5_160x64;
    return true;
  }
  return false;
}

bool parseOrientation(const char* text, Orientation& out) {
  if (sameText(text, "LANDSCAPE")) {
    out = Orientation::Landscape;
    return true;
  }
  if (sameText(text, "PORTRAIT")) {
    out = Orientation::Portrait;
    return true;
  }
  return false;
}

bool parseLineScale(const char* text, LineScaleMode& out) {
  if (sameText(text, "FILL")) {
    out = LineScaleMode::Fill;
    return true;
  }
  if (sameText(text, "HERO")) {
    out = LineScaleMode::Hero;
    return true;
  }
  return false;
}

bool parseDisplayContent(const char* text, Core::DisplayContent& out) {
  if (text == nullptr) return false;
  for (uint8_t index = 0; index <= static_cast<uint8_t>(Core::DisplayContent::Blank); index++) {
    const Core::DisplayContent candidate = static_cast<Core::DisplayContent>(index);
    if (sameText(text, Core::name(candidate))) {
      out = candidate;
      return true;
    }
  }
  return false;
}

uint8_t parseLines(const char* text, Core::DisplayContent* lines, uint8_t maxLines) {
  if (text == nullptr || lines == nullptr || maxLines == 0) return 0;
  uint8_t count = 0;
  uint8_t cursor = 0;
  while (text[cursor] != '\0' && count < maxLines) {
    while (text[cursor] == ' ' || text[cursor] == ',') cursor++;
    if (text[cursor] == '\0') break;
    char token[16] = {};
    uint8_t length = 0;
    while (text[cursor] != '\0' && text[cursor] != ',' && length + 1 < sizeof(token)) {
      if (text[cursor] != ' ') token[length++] = text[cursor];
      cursor++;
    }
    token[length] = '\0';
    Core::DisplayContent content = Core::DisplayContent::Clock;
    if (parseDisplayContent(token, content)) {
      lines[count++] = content;
    }
  }
  return count;
}

void formatLines(const Core::DisplayContent* lines, uint8_t count, char* out, uint8_t outSize) {
  if (out == nullptr || outSize == 0) return;
  out[0] = '\0';
  uint8_t used = 0;
  for (uint8_t index = 0; index < count; index++) {
    const char* label = Core::name(lines[index]);
    uint8_t length = 0;
    while (label[length] != '\0') length++;
    if (index > 0) {
      if (used + 2 >= outSize) break;
      out[used++] = ',';
    }
    if (used + length + 1 > outSize) break;
    for (uint8_t cursor = 0; cursor < length; cursor++) out[used++] = label[cursor];
    out[used] = '\0';
  }
}

LayoutPlan planLayout(const Geometry& geometry, const Core::DisplayContent* lines, uint8_t lineCount,
                      LineScaleMode scaleMode, uint8_t heroLine) {
  LayoutPlan plan;
  plan.geometry = geometry;
  const uint8_t maxLines = maxLinesFor(geometry);
  uint8_t used = 0;
  if (lines != nullptr) {
    for (uint8_t index = 0; index < lineCount && used < maxLines; index++) {
      plan.lines[used++].content = lines[index];
    }
  }
  if (used == 0) {
    plan.lines[0].content = Core::DisplayContent::Clock;
    used = 1;
  }
  plan.lineCount = used;

  uint16_t heights[MAX_CONTENT_LINES] = {};
  if (used == 1) {
    heights[0] = geometry.rows;
  } else if (scaleMode == LineScaleMode::Hero && heroLine < used) {
    const uint8_t others = static_cast<uint8_t>(used - 1);
    uint16_t otherH = geometry.rows / static_cast<uint16_t>(used + 1);
    if (otherH < 1) otherH = 1;
    uint16_t heroH = geometry.rows > otherH * others ? static_cast<uint16_t>(geometry.rows - otherH * others) : otherH;
    if (heroH < otherH) heroH = otherH;
    uint16_t usedHeight = 0;
    for (uint8_t index = 0; index < used; index++) {
      if (index + 1 == used) {
        heights[index] = geometry.rows > usedHeight ? static_cast<uint16_t>(geometry.rows - usedHeight) : 1;
      } else {
        heights[index] = index == heroLine ? heroH : otherH;
        usedHeight = static_cast<uint16_t>(usedHeight + heights[index]);
      }
    }
  } else {
    uint16_t base = geometry.rows / used;
    if (base < 1) base = 1;
    uint16_t usedHeight = 0;
    for (uint8_t index = 0; index < used; index++) {
      if (index + 1 == used) {
        heights[index] = geometry.rows > usedHeight ? static_cast<uint16_t>(geometry.rows - usedHeight) : 1;
      } else {
        heights[index] = base;
        usedHeight = static_cast<uint16_t>(usedHeight + base);
      }
    }
  }

  uint16_t originY = 0;
  for (uint8_t index = 0; index < used; index++) {
    plan.lines[index].x = 0;
    plan.lines[index].y = originY;
    plan.lines[index].width = geometry.columns;
    plan.lines[index].height = heights[index] == 0 ? 1 : heights[index];
    uint8_t scale = static_cast<uint8_t>(plan.lines[index].height / TILE_ROWS);
    if (scale < 1) scale = 1;
    plan.lines[index].scale = scale;
    originY = static_cast<uint16_t>(originY + plan.lines[index].height);
  }
  return plan;
}

Hub75Canvas hub75Canvas(PanelPreset preset) {
  Hub75Canvas canvas;
  const uint8_t count = stackedP5(preset);
  if (count == 0) {
    canvas.width = 64;
    canvas.height = 32;
    canvas.chain = 1;
    return canvas;
  }
  canvas.width = static_cast<uint16_t>(count * 64);
  canvas.height = 32;
  canvas.chain = count;
  return canvas;
}

bool mapLogicalToHub75(const Geometry& geometry, uint16_t x, uint16_t y, uint16_t& dmaX, uint16_t& dmaY) {
  if (x >= geometry.columns || y >= geometry.rows) return false;
  uint16_t landscapeX = x;
  uint16_t landscapeY = y;
  if (geometry.orientation == Orientation::Portrait && geometry.preset != PanelPreset::Led32x16) {
    uint16_t landColumns = 0;
    uint16_t landRows = 0;
    landscapeSize(geometry.preset, landColumns, landRows);
    landscapeX = static_cast<uint16_t>(landColumns - 1 - y);
    landscapeY = x;
  }
  return mapLandscapeToHub75(geometry.preset, landscapeX, landscapeY, dmaX, dmaY);
}

uint16_t ws2812Index(const Geometry& geometry, uint16_t x, uint16_t y) {
  if (geometry.columns == TILE_COLUMNS && geometry.rows == TILE_ROWS) {
    return ledIndex(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
  }
  if (x >= geometry.columns || y >= geometry.rows) return 0;

  const uint8_t tileX = static_cast<uint8_t>(x / PANEL_COLUMNS);
  const uint8_t tileY = static_cast<uint8_t>(y / PANEL_ROWS);
  const uint8_t tilesX = static_cast<uint8_t>(geometry.columns / PANEL_COLUMNS);
  const uint8_t tilesY = static_cast<uint8_t>(geometry.rows / PANEL_ROWS);
  uint8_t localX = static_cast<uint8_t>(x % PANEL_COLUMNS);
  uint8_t localY = static_cast<uint8_t>(y % PANEL_ROWS);
  if ((tileY % 2) == 0) {
    localX = static_cast<uint8_t>(PANEL_COLUMNS - 1 - localX);
    localY = static_cast<uint8_t>(PANEL_ROWS - 1 - localY);
  }
  if (localX & 1) localY = static_cast<uint8_t>(PANEL_ROWS - 1 - localY);
  const uint8_t panel = static_cast<uint8_t>((tilesY - 1 - tileY) * tilesX + tileX);
  return static_cast<uint16_t>(panel * PANEL_COLUMNS * PANEL_ROWS + localX * PANEL_ROWS + localY);
}

}  // namespace DisplayLogic
