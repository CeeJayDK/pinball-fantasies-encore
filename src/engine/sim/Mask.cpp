#include "engine/sim/Mask.h"

#include <algorithm>
#include <cstring>

namespace encore {

Mask::Mask(int width, int height)
    : width_(width), height_(height), pitch_((width + 7) / 8), bits_(static_cast<std::size_t>(pitch_) * height, 0) {}

Mask Mask::fromPacked(ByteView packed, int width, int height, int pitch) {
  Mask m;
  m.width_ = width;
  m.height_ = height;
  m.pitch_ = pitch;
  m.bits_.assign(static_cast<std::size_t>(pitch) * height, 0);
  std::memcpy(m.bits_.data(), packed.data(), std::min(m.bits_.size(), packed.size()));
  return m;
}

void Mask::set(int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  u8& b = bits_[static_cast<std::size_t>(y) * pitch_ + (x >> 3)];
  const u8 bit = static_cast<u8>(0x80u >> (x & 7));
  if (on) b |= bit; else b &= static_cast<u8>(~bit);
}

void Mask::patch(int byteX, int y, int byteW, int h, ByteView packedRows) {
  for (int r = 0; r < h; ++r) {
    const int yy = y + r;
    if (yy < 0 || yy >= height_) continue;
    for (int c = 0; c < byteW; ++c) {
      const int bx = byteX + c;
      const std::size_t src = static_cast<std::size_t>(r) * byteW + c;
      if (bx < 0 || bx >= pitch_ || src >= packedRows.size()) continue;
      bits_[static_cast<std::size_t>(yy) * pitch_ + bx] = packedRows[src];
    }
  }
}

void Mask::orWith(const Mask& other) {
  const std::size_t n = std::min(bits_.size(), other.bits_.size());
  for (std::size_t i = 0; i < n; ++i) bits_[i] |= other.bits_[i];
}

}  // namespace encore
