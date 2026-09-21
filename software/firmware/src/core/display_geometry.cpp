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

void landscapeSize(PanelPreset preset, uint16_t& columns, uint16_t& rows) {
  switch (preset) {
    case PanelPreset::P5_64x32:
      columns = 64;
      rows = 32;
      break;
    case PanelPreset::P5_96x64:
      columns = 96;
      rows = 64;
      break;
    case PanelPreset::P5_128x64:
      columns = 128;
      rows = 64;
      break;
    case PanelPreset::Led32x16:
    default:
      columns = TILE_COLUMNS;
      rows = TILE_ROWS;
      break;
  }
}

const Core::DisplayContent kDefaultOrder[] = {
    Core::DisplayContent::Clock,      Core::DisplayContent::ClockAndEnd, Core::DisplayContent::Score,
    Core::DisplayContent::ArrowCount, Core::DisplayContent::SetPoints,   Core::DisplayContent::Shooter};

bool mapLandscapeToHub75(PanelPreset preset, uint16_t x, uint16_t y, uint16_t& dmaX, uint16_t& dmaY) {
  uint16_t columns = 0;
  uint16_t rows = 0;
  landscapeSize(preset, columns, rows);
  if (x >= columns || y >= rows) return false;

  switch (preset) {
    case PanelPreset::P5_96x64:
      // U-shape chain: P0 top 64x32, P1 right 64x32 rotated 90° CW, P2 bottom
      // 64x32 under the top. The HUB75 ribbon runs top → right → bottom.
      if (x < 64) {
        if (y < 32) {
          dmaX = x;
          dmaY = y;
        } else {
          dmaX = static_cast<uint16_t>(128 + x);
          dmaY = static_cast<uint16_t>(y - 32);
        }
      } else {
        const uint16_t localX = static_cast<uint16_t>(x - 64);
        dmaX = static_cast<uint16_t>(64 + y);
        dmaY = static_cast<uint16_t>(31 - localX);
      }
      return true;

    case PanelPreset::P5_128x64:
      if (y < 32) {
        dmaX = x;
        dmaY = y;
      } else {
        dmaX = static_cast<uint16_t>(128 + x);
        dmaY = static_cast<uint16_t>(y - 32);
      }
      return true;

    case PanelPreset::P5_64x32:
    case PanelPreset::Led32x16:
    default:
      dmaX = x;
      dmaY = y;
      return true;
  }
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

uint8_t maxLinesFor(const Geometry& geometry) {
  if (geometry.rows < TILE_ROWS) return 1;
  uint8_t lines = static_cast<uint8_t>(geometry.rows / TILE_ROWS);
  if (lines > MAX_CONTENT_LINES) lines = MAX_CONTENT_LINES;
  return lines == 0 ? 1 : lines;
}

uint8_t defaultLineCount(const Geometry& geometry) {
  const uint8_t maxLines = maxLinesFor(geometry);
  if (maxLines <= 1) return 1;
  if (geometry.orientation == Orientation::Portrait) return maxLines;
  if (maxLines >= 4) return 2;
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

bool validPreset(uint8_t value) { return value <= static_cast<uint8_t>(PanelPreset::P5_128x64); }

bool validOrientation(uint8_t value) { return value <= static_cast<uint8_t>(Orientation::Portrait); }

const char* name(PanelPreset preset) {
  switch (preset) {
    case PanelPreset::Led32x16: return "LED_32X16";
    case PanelPreset::P5_64x32: return "P5_64X32";
    case PanelPreset::P5_96x64: return "P5_96X64";
    case PanelPreset::P5_128x64: return "P5_128X64";
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

bool parsePreset(const char* text, PanelPreset& out) {
  if (sameText(text, "LED_32X16")) {
    out = PanelPreset::Led32x16;
    return true;
  }
  if (sameText(text, "P5_64X32")) {
    out = PanelPreset::P5_64x32;
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

LayoutPlan planLayout(const Geometry& geometry, const Core::DisplayContent* lines, uint8_t lineCount) {
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

  uint8_t scale = 1;
  const uint8_t scaleX = geometry.columns / TILE_COLUMNS;
  const uint8_t scaleY = geometry.rows / static_cast<uint16_t>(TILE_ROWS * used);
  const uint8_t limit = scaleX < scaleY ? scaleX : scaleY;
  if (limit > 1) scale = limit;

  const uint16_t blockWidth = static_cast<uint16_t>(TILE_COLUMNS * scale);
  const uint16_t blockHeight = static_cast<uint16_t>(TILE_ROWS * scale * used);
  const uint16_t originX =
      geometry.columns > blockWidth ? static_cast<uint16_t>((geometry.columns - blockWidth) / 2) : 0;
  uint16_t originY = 0;
  if (used == 1 && geometry.rows > blockHeight) {
    originY = static_cast<uint16_t>((geometry.rows - blockHeight) / 2);
  }

  for (uint8_t index = 0; index < used; index++) {
    plan.lines[index].scale = scale;
    plan.lines[index].x = originX;
    plan.lines[index].y = static_cast<uint16_t>(originY + index * TILE_ROWS * scale);
  }
  return plan;
}

Hub75Canvas hub75Canvas(PanelPreset preset) {
  Hub75Canvas canvas;
  switch (preset) {
    case PanelPreset::P5_96x64:
      // Three 64x32 modules in a U, driven as one 192x32 chain.
      canvas.width = 192;
      canvas.height = 32;
      canvas.chain = 3;
      break;
    case PanelPreset::P5_128x64:
      canvas.width = 256;
      canvas.height = 32;
      canvas.chain = 4;
      break;
    case PanelPreset::P5_64x32:
    case PanelPreset::Led32x16:
    default:
      canvas.width = 64;
      canvas.height = 32;
      canvas.chain = 1;
      break;
  }
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
