#pragma once
// The ball is a "compiled sprite": the original does not store its pixels anywhere, it
// stores a routine whose instructions carry each pixel's colour as an immediate operand.
// Decoding that routine recovers the image.
#include "core/Types.h"

namespace pfr {

struct BallSprite {
  int width = 0;
  int height = 0;
  Bytes pixels;   ///< width * height palette indices
  Bytes opaque;   ///< width * height flags; the ball is a disc, so the corners are empty

  bool valid() const { return width > 0 && height > 0; }
  u8 at(int x, int y) const { return pixels[static_cast<std::size_t>(y) * width + x]; }
  bool covers(int x, int y) const { return opaque[static_cast<std::size_t>(y) * width + x] != 0; }

  /// Recovers the sprite from a table's code segment.
  static BallSprite decode(ByteView code);
};

}  // namespace pfr
