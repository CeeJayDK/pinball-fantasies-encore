#include "gfx/Framebuffer.h"

#include <algorithm>
#include <cstring>

namespace pfr {

Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * height, 0) {}

void Framebuffer::clear(u8 index) { std::fill(pixels_.begin(), pixels_.end(), index); }

void Framebuffer::fillRect(Rect r, u8 index) {
  const int x0 = std::max(r.x, 0), y0 = std::max(r.y, 0);
  const int x1 = std::min(r.x + r.w, width_), y1 = std::min(r.y + r.h, height_);
  for (int y = y0; y < y1; ++y) std::memset(row(y) + x0, index, static_cast<std::size_t>(std::max(0, x1 - x0)));
}

void Framebuffer::blit(const u8* src, int srcPitch, int w, int h, int dx, int dy) {
  const int x0 = std::max(dx, 0), y0 = std::max(dy, 0);
  const int x1 = std::min(dx + w, width_), y1 = std::min(dy + h, height_);
  if (x1 <= x0 || y1 <= y0) return;
  for (int y = y0; y < y1; ++y)
    std::memcpy(row(y) + x0, src + static_cast<std::size_t>(y - dy) * srcPitch + (x0 - dx), static_cast<std::size_t>(x1 - x0));
}

void Framebuffer::blitKeyed(const u8* src, int srcPitch, int w, int h, int dx, int dy, u8 transparent) {
  const int x0 = std::max(dx, 0), y0 = std::max(dy, 0);
  const int x1 = std::min(dx + w, width_), y1 = std::min(dy + h, height_);
  if (x1 <= x0 || y1 <= y0) return;
  for (int y = y0; y < y1; ++y) {
    const u8* s = src + static_cast<std::size_t>(y - dy) * srcPitch + (x0 - dx);
    u8* d = row(y) + x0;
    for (int x = 0; x < x1 - x0; ++x)
      if (s[x] != transparent) d[x] = s[x];
  }
}

}  // namespace pfr
