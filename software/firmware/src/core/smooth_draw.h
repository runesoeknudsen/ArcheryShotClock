#pragma once

#include "display_logic.h"

// Grotesque-sans glyphs for cabinets larger than 32x16. The low-resolution
// bitmap font stays on that original panel; everything else is drawn as
// capsules, ellipses and arcs at the destination size so a curve reads as a
// hill instead of a staircase of upscaled pixels.

namespace DisplayLogic {

void drawSmoothLine(const RenderRequest& request, Core::DisplayContent content, uint16_t destX,
                    uint16_t destY, uint16_t destW, uint16_t destH, uint16_t columns, uint16_t rows,
                    uint32_t* pixels);

}  // namespace DisplayLogic
