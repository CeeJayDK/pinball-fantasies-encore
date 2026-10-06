#pragma once
// The dot-matrix display's fonts and colours.
//
// The display is a grid of 160 by 16 dots shown in a 320 by 33 pixel band, with each dot a
// single pixel on every other column and every other row, which is what gives it the gaps
// between dots. Characters are seven dots wide in a cell of eight, the last being a gap.
#include <array>
#include <map>

#include "core/Types.h"

namespace encore {

enum class DmFontSize { Height5, Height8, Height11, Height13 };

struct DmFont {
  int height = 0;
  /// One entry per character; each row is a bit pattern with the leftmost dot at 0x80.
  std::map<char, std::vector<u8>> glyphs;

  bool has(char c) const { return glyphs.count(c) != 0; }
};

struct DmPalette {
  u8 indexOff = 0;   ///< palette entry for an unlit dot
  u8 indexOn = 0;    ///< palette entry for a lit dot
  Rgb colorOff;
  Rgb colorOn;
};

struct DmAssets {
  std::array<DmFont, 4> fonts;   ///< in the order of DmFontSize
  DmPalette palette;

  const DmFont& font(DmFontSize size) const { return fonts[static_cast<std::size_t>(size)]; }
};

/// Reads the fonts and the display's two colours from a table's data segment.
DmAssets extractDmAssets(ByteView dataSegment, int tableIndex);

}  // namespace encore
