#pragma once
// Options and high scores, stored in the DOS game's own formats (PINBALL.CFG, TABLEn.HI)
// so they stay interchangeable with the original (translated from pfr's config.rs).
#include <array>
#include <filesystem>

#include "assets/Bcd.h"

namespace pfr {

enum class ScrollSpeed : u8 { Hard, Medium, Soft };
inline i16 rawScrollSpeed(ScrollSpeed s) { return s == ScrollSpeed::Hard ? 20 : s == ScrollSpeed::Medium ? 11 : 9; }

enum class Resolution : u8 { Normal, High, Full };

struct Options {
  u8 balls = 3;
  bool angleHigh = true;
  ScrollSpeed scrollSpeed = ScrollSpeed::Medium;
  Resolution resolution = Resolution::Normal;
  bool noMusic = false;
  bool mono = false;
};

struct HighScore {
  Bcd score;
  std::array<u8, 3> name{};
};
using HighScores = std::array<HighScore, 4>;

struct Config {
  Options options;
  std::array<HighScores, 4> highScores;

  static Config defaults();
  /// Reads PINBALL.CFG and TABLEn.HI from `dir`, or failing that from `fallbackDir` (the
  /// game folder, so a player's DOS high scores carry over), keeping defaults otherwise.
  static Config load(const std::filesystem::path& dir, const std::filesystem::path& fallbackDir = {});
  static void saveOptions(const std::filesystem::path& dir, const Options& o);
  static void saveHighScores(const std::filesystem::path& dir, int table, const HighScores& s);
};

}  // namespace pfr
