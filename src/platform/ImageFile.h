#pragma once
// Reads a picture file for the renderer. PNG only, through this project's own reader, so
// that it works the same on every platform.
#include <filesystem>
#include <optional>

#include "core/Types.h"

namespace pfr {

struct RgbaImage {
  int width = 0, height = 0;
  Bytes pixels;  ///< RGBA, top row first, colours already multiplied by alpha
};

std::optional<RgbaImage> loadImageFile(const std::filesystem::path& path);

}  // namespace pfr
