#pragma once
// Locates the original Pinball Fantasies files. The remake reads the data from the
// user's own copy of the game; nothing from the original is redistributed.
#include <array>
#include <filesystem>

#include "core/Types.h"

namespace encore {

struct GameFiles {
  std::filesystem::path directory;
  std::filesystem::path intro;                      ///< INTRO.PRG
  std::array<std::filesystem::path, 4> tables;      ///< TABLE1..4.PRG
  std::array<std::filesystem::path, 4> tableMusic;  ///< TABLE1..4.MOD
  std::filesystem::path introMusic;                 ///< INTRO.MOD
  std::filesystem::path menuMusic;                  ///< MOD2.MOD

  /// Builds the set from a known directory; throws DataError when required files are missing.
  static GameFiles fromDirectory(const std::filesystem::path& dir);
};

}  // namespace encore
