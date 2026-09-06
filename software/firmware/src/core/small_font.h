#pragma once

// Compact glyphs for the 32x16 panel. 3x5 letters sit beside the clock;
// 5x5 letters spell END / SCORE on their own row.

namespace SmallFont {

constexpr uint8_t NARROW_WIDTH = 3;
constexpr uint8_t NARROW_HEIGHT = 5;
constexpr uint8_t WIDE_WIDTH = 5;
constexpr uint8_t WIDE_HEIGHT = 5;

// Indexed by letter - 'A'. Unused slots are blank.
constexpr char NARROW[8][NARROW_HEIGHT][NARROW_WIDTH + 1] = {
    {// A
     " # ", "# #", "###", "# #", "# #"},
    {// B
     "## ", "# #", "## ", "# #", "## "},
    {// C
     " ##", "#  ", "#  ", "#  ", " ##"},
    {// D
     "## ", "# #", "# #", "# #", "## "},
    {// E
     "###", "#  ", "## ", "#  ", "###"},
    {// F
     "###", "#  ", "## ", "#  ", "#  "},
    {// G
     " ##", "#  ", "# #", "# #", " ##"},
    {// H
     "# #", "# #", "###", "# #", "# #"},
};

constexpr char WIDE_A[WIDE_HEIGHT][WIDE_WIDTH + 1] = {" ### ", "#   #", "#####", "#   #", "#   #"};
constexpr char WIDE_C[WIDE_HEIGHT][WIDE_WIDTH + 1] = {" ### ", "#    ", "#    ", "#    ", " ### "};
constexpr char WIDE_D[WIDE_HEIGHT][WIDE_WIDTH + 1] = {"#### ", "#   #", "#   #", "#   #", "#### "};
constexpr char WIDE_E[WIDE_HEIGHT][WIDE_WIDTH + 1] = {"#####", "#    ", "#### ", "#    ", "#####"};
constexpr char WIDE_N[WIDE_HEIGHT][WIDE_WIDTH + 1] = {"#   #", "##  #", "# # #", "#  ##", "#   #"};
constexpr char WIDE_O[WIDE_HEIGHT][WIDE_WIDTH + 1] = {" ### ", "#   #", "#   #", "#   #", " ### "};
constexpr char WIDE_R[WIDE_HEIGHT][WIDE_WIDTH + 1] = {"#### ", "#   #", "#### ", "#  # ", "#   #"};
constexpr char WIDE_S[WIDE_HEIGHT][WIDE_WIDTH + 1] = {" ####", "#    ", " ### ", "    #", "#### "};

inline const char* wideRow(char letter, uint8_t row) {
  switch (letter) {
    case 'A': return WIDE_A[row];
    case 'C': return WIDE_C[row];
    case 'D': return WIDE_D[row];
    case 'E': return WIDE_E[row];
    case 'N': return WIDE_N[row];
    case 'O': return WIDE_O[row];
    case 'R': return WIDE_R[row];
    case 'S': return WIDE_S[row];
    default: return "     ";
  }
}

}  // namespace SmallFont
