#pragma once
// Minimal PNG writer (stored deflate blocks) and reader (its own inflate), no dependencies:
// the writer serves the tools and tests, the reader the replacement pictures at every size.
#include <filesystem>
#include <optional>

#include "core/Types.h"

namespace pfr {

struct RgbaPng {
  int width = 0, height = 0;
  Bytes pixels;  ///< RGBA, top row first
};

/// Reads an 8-bit PNG (grey, palette, RGB or RGBA, not interlaced) as RGBA.
std::optional<RgbaPng> readPng(const std::filesystem::path& path);

bool writeIndexedPng(const std::filesystem::path& path, const u8* pixels, int width, int height, const std::vector<Rgb>& palette);
bool writeRgbPng(const std::filesystem::path& path, const u8* rgb, int width, int height, bool flipVertically = false);

}  // namespace pfr
