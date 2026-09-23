#include "platform/ImageFile.h"

#include <optional>
#include <utility>

#include "core/Png.h"

namespace pfr {

std::optional<RgbaImage> loadImageFile(const std::filesystem::path& path) {
  auto png = readPng(path);
  if (!png) return std::nullopt;
  RgbaImage out;
  out.width = png->width;
  out.height = png->height;
  out.pixels = std::move(png->pixels);
  // The renderer blends with the colours already multiplied by alpha.
  for (std::size_t i = 0; i < out.pixels.size(); i += 4) {
    const u32 a = out.pixels[i + 3];
    for (int c = 0; c < 3; ++c)
      out.pixels[i + static_cast<std::size_t>(c)] = static_cast<u8>((out.pixels[i + static_cast<std::size_t>(c)] * a + 127) / 255);
  }
  return out;
}

}  // namespace pfr
