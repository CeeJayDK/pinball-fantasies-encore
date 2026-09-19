#pragma once
// Reads PNG, JPEG and other common picture files through macOS ImageIO.
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
