#pragma once
// Where the tests find the game's files: ENCORE_DATA, or the FANTASY folder beside the project.
#include <cstdlib>
#include <filesystem>

namespace test {
inline std::filesystem::path gameDir() {
  if (const char* d = std::getenv("ENCORE_DATA")) return d;
  return std::filesystem::path(ENCORE_SOURCE_DIR) / ".." / ".." / "FANTASY";
}
}  // namespace test
