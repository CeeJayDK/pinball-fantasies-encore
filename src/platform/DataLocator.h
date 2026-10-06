#pragma once
// Finds the player's copy of the original game files, so the app can be launched by
// double-clicking it rather than only from a terminal with an argument.
#include <filesystem>
#include <optional>
#include <string>

namespace encore {

/// The folder the game files are read from: FANTASY, inside this version's own folder
/// (see preferencesDir), and nowhere else -- or the folder given with --data, when there
/// is one. Nothing is searched for, so nothing outside this version's own things is read.
std::optional<std::filesystem::path> locateGameData(const std::optional<std::filesystem::path>& explicitDir);

/// Where the game files belong: <preferences>/FANTASY.
std::filesystem::path gameDataDir();

/// Where the app keeps its own files: the game files, options, high scores.
std::filesystem::path preferencesDir();

/// Tells the player what is missing.
void reportMissingGameData();

/// Says what went wrong, for a failure that would otherwise end the program in silence.
void reportError(const std::string& message);

}  // namespace encore
