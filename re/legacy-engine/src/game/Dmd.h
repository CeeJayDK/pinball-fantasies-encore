#pragma once
// The dot-matrix display: a grid of 160 by 16 dots shown across the bottom of the screen.
#include "data/DmFont.h"
#include "gfx/Framebuffer.h"

namespace pfr {

class Dmd {
 public:
  static constexpr int kDotsWide = 160;
  static constexpr int kDotsHigh = 16;
  static constexpr int kCellWidth = 8;   ///< a character cell, the last dot being a gap
  static constexpr int kPixelRows = 33;

  void setAssets(const DmAssets* assets) { assets_ = assets; }

  void clear();
  void setDot(int x, int y, bool lit);
  bool dot(int x, int y) const;

  /// Draws text with its top-left at the given dot position. Returns the width used.
  int drawText(int x, int y, std::string_view text, DmFontSize size);
  /// Draws text centred horizontally.
  void drawCentred(int y, std::string_view text, DmFontSize size);
  /// How wide the text would be, in dots.
  int measure(std::string_view text, DmFontSize size) const;

  /// Paints the display into the bottom band of a frame.
  void render(Framebuffer& frame) const;

 private:
  const DmAssets* assets_ = nullptr;
  std::array<std::array<bool, kDotsWide>, kDotsHigh> dots_{};
};

}  // namespace pfr
