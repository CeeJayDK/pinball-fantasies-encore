#pragma once
// The engine reads the game's data at fixed addresses, so it needs exactly the file
// version it was written from: the 1994 disk release. Other releases (for
// example cracked re-releases) are laid out differently and are refused up front.
#include <filesystem>
#include <string>
#include <vector>

namespace encore {

/// The files that differ from the supported version, or cannot be read (empty: all good).
/// INTRO.MOD is not checked: the original game rewrites it as part of its copy protection.
std::vector<std::string> unsupportedGameFiles(const std::filesystem::path& dir);

}  // namespace encore
