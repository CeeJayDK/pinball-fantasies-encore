#pragma once
// Finds the player's copy of the original game files, so the app can be launched by
// double-clicking it rather than only from a terminal with an argument.
#include <filesystem>
#include <optional>

namespace pfr {

/// Searches, in order: an explicit folder, the folder remembered from last time, the
/// working directory and its parents, and the folders around the application bundle.
std::optional<std::filesystem::path> locateGameData(const std::optional<std::filesystem::path>& explicitDir);

/// Asks the player to point at the folder, using the system's folder chooser.
/// Returns nothing if they cancel or the folder does not hold the game.
std::optional<std::filesystem::path> askForGameData();

/// Where the app keeps its own files: remembered folder, options, high scores.
std::filesystem::path preferencesDir();

/// Remembers a folder for next time.
void rememberGameData(const std::filesystem::path& dir);

/// Tells the player what is missing.
void reportMissingGameData();

}  // namespace pfr
