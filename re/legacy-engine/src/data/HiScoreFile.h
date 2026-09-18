#pragma once
// TABLEn.HI: four entries of 16 bytes = 12 decimal digits (one per byte, most significant
// first) followed by a 3-character name and a zero byte. We read and write the exact
// format so scores stay compatible with the original game.
#include <array>
#include <filesystem>

#include "core/Types.h"

namespace pfr {

struct HiScoreEntry {
  u64 score = 0;
  std::string name;  ///< up to 3 characters
};

struct HiScoreTable {
  static constexpr int kEntries = 4;
  std::array<HiScoreEntry, kEntries> entries;

  /// The defaults the original game uses when no file exists.
  static HiScoreTable defaults();
  static HiScoreTable decode(ByteView bytes);
  Bytes encode() const;

  /// Returns the rank (0..3) a score would take, or -1 if it does not qualify.
  int rankFor(u64 score) const;
  /// Inserts at the rank position, shifting lower entries down.
  void insert(int rank, const HiScoreEntry& entry);
};

HiScoreTable loadHiScores(const std::filesystem::path& path);
bool saveHiScores(const std::filesystem::path& path, const HiScoreTable& table);

}  // namespace pfr
