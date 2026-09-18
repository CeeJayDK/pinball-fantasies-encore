#pragma once
// Decoder for the IFF ILBM / PBM pictures embedded in the executables (Deluxe Paint output).
#include <array>
#include <optional>

#include "core/Types.h"

namespace pfr {

struct ColorRange {  // CRNG chunk: Deluxe Paint colour cycling
  u16 rate = 0;      ///< 16384 = 60 steps per second
  u16 flags = 0;     ///< bit 0 active, bit 1 reverse
  u8 low = 0, high = 0;
};

struct IffImage {
  int width = 0;
  int height = 0;
  int planes = 0;
  u8 masking = 0;
  u8 transparent = 0;
  bool chunky = false;  ///< true for "PBM " (8-bit chunky), false for planar ILBM
  std::vector<Rgb> palette;
  std::vector<ColorRange> ranges;
  Bytes pixels;  ///< width*height palette indices, row-major

  u8 at(int x, int y) const { return pixels[static_cast<std::size_t>(y) * width + x]; }
};

/// Parses one FORM at `data` (which must begin with "FORM"). Returns nullopt if not an image FORM.
std::optional<IffImage> decodeIff(ByteView data);

/// Finds every image FORM inside a byte blob (used to pull pictures out of the executables).
struct IffLocation {
  std::size_t offset;
  std::size_t length;
};
std::vector<IffLocation> findIffForms(ByteView blob);

}  // namespace pfr
