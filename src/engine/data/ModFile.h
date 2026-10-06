#pragma once
// ProTracker "M.K." module parser (4 channels, 31 samples, 64-row patterns).
#include <array>
#include <filesystem>
#include <optional>

#include "core/Types.h"

namespace encore {

struct ModSample {
  std::string name;
  u32 length = 0;      ///< bytes
  i8 finetune = 0;     ///< -8..7
  u8 volume = 64;
  u32 loopStart = 0;   ///< bytes
  u32 loopLength = 0;  ///< bytes; <= 2 means no loop
  std::vector<i8> data;
  bool loops() const { return loopLength > 2; }
};

struct ModNote {
  u8 sample = 0;   ///< 1..31, 0 = none
  u16 period = 0;  ///< Amiga period, 0 = none
  u8 effect = 0;   ///< 0..15
  u8 param = 0;
};

struct ModFile {
  std::string title;
  std::array<ModSample, 31> samples;
  u8 songLength = 0;
  u8 restart = 0;
  std::array<u8, 128> order{};
  int patternCount = 0;
  std::vector<ModNote> notes;  ///< patternCount * 64 rows * 4 channels

  const ModNote& note(int pattern, int row, int channel) const {
    return notes[(static_cast<std::size_t>(pattern) * 64 + row) * 4 + channel];
  }

  static ModFile load(const std::filesystem::path& path);
  static ModFile parse(ByteView bytes, std::string name);
};

}  // namespace encore
