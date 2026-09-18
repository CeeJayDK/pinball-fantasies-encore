#pragma once
// 1-bit-per-pixel bitmap as stored in the game: rows of ceil(width/8) bytes, most
// significant bit = leftmost pixel, 1 = solid. Used for playfield collision, flipper
// frames and ball occlusion.
#include "core/Types.h"

namespace pfr {

class Mask {
 public:
  Mask() = default;
  Mask(int width, int height);
  /// Wraps a copy of packed rows (`pitch` bytes per row).
  static Mask fromPacked(ByteView packed, int width, int height, int pitch);

  int width() const { return width_; }
  int height() const { return height_; }
  int pitch() const { return pitch_; }
  bool get(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return false;
    return (bits_[static_cast<std::size_t>(y) * pitch_ + (x >> 3)] >> (7 - (x & 7))) & 1u;
  }
  void set(int x, int y, bool on);
  /// Replaces a rectangle of packed bytes (byte-aligned x), the way the game patches masks.
  void patch(int byteX, int y, int byteW, int h, ByteView packedRows);
  void orWith(const Mask& other);
  ByteView packed() const { return bits_; }
  Bytes& packedMutable() { return bits_; }
  const u8* rowBytes(int y) const { return bits_.data() + static_cast<std::size_t>(y) * pitch_; }

 private:
  int width_ = 0, height_ = 0, pitch_ = 0;
  Bytes bits_;
};

}  // namespace pfr
