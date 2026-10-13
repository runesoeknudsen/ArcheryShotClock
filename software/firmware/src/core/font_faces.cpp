#include "font_face.h"

#include "generated/font_sans_serif.h"

namespace DisplayLogic {

// Point this at another generated kFont… to try a different face.
const FontFace& activeFontFace() { return kFontSansSerif; }

}  // namespace DisplayLogic
