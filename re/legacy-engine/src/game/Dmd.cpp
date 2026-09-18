#include "game/Dmd.h"

#include <algorithm>

namespace pfr {

void Dmd::clear() {
  for (auto& row : dots_) row.fill(false);
}

void Dmd::setDot(int x, int y, bool lit) {
  if (x < 0 || y < 0 || x >= kDotsWide || y >= kDotsHigh) return;
  dots_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = lit;
}

bool Dmd::dot(int x, int y) const {
  if (x < 0 || y < 0 || x >= kDotsWide || y >= kDotsHigh) return false;
  return dots_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

int Dmd::measure(std::string_view text, DmFontSize size) const {
  if (!assets_) return 0;
  const DmFont& font = assets_->font(size);
  int width = 0;
  for (char c : text)
    if (font.has(c) || c == ' ') width += kCellWidth;
  return width;
}

int Dmd::drawText(int x, int y, std::string_view text, DmFontSize size) {
  if (!assets_) return 0;
  const DmFont& font = assets_->font(size);
  const int start = x;
  for (char c : text) {
    const auto found = font.glyphs.find(c);
    if (found == font.glyphs.end()) continue;
    const std::vector<u8>& rows = found->second;
    for (int row = 0; row < static_cast<int>(rows.size()); ++row)
      for (int column = 0; column < 7; ++column)
        if (rows[static_cast<std::size_t>(row)] & (0x80u >> column)) setDot(x + column, y + row, true);
    x += kCellWidth;
  }
  return x - start;
}

void Dmd::drawCentred(int y, std::string_view text, DmFontSize size) {
  drawText((kDotsWide - measure(text, size)) / 2, y, text, size);
}

void Dmd::render(Framebuffer& frame) const {
  if (!assets_) return;
  // The band sits below the playfield window. Every dot is one pixel, spaced two apart in
  // both directions, which is what makes the gaps between them.
  const int top = frame.height() - kPixelRows;
  frame.fillRect({0, top, frame.width(), kPixelRows}, 0);
  for (int y = 0; y < kDotsHigh; ++y) {
    const int row = top + 2 + 2 * y;
    for (int x = 0; x < kDotsWide; ++x)
      frame.put(x * 2, row, dots_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]
                                ? assets_->palette.indexOn
                                : assets_->palette.indexOff);
  }
}

}  // namespace pfr
