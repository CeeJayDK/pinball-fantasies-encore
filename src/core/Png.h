#pragma once
// Minimal PNG writer (stored deflate blocks, no dependencies) for tooling and tests.
#include <filesystem>

#include "core/Types.h"

namespace pfr {

bool writeIndexedPng(const std::filesystem::path& path, const u8* pixels, int width, int height, const std::vector<Rgb>& palette);
bool writeRgbPng(const std::filesystem::path& path, const u8* rgb, int width, int height, bool flipVertically = false);

}  // namespace pfr
