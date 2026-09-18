#pragma once
// A ProTracker module decoded into notes with their effects already classified, as the
// game's own player sees them (translated from pfr's sound/loader.rs).
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/Types.h"

namespace pfr {

struct SongNote {
  enum class Tone : u8 { None, Arpeggio, Portamento, Vibrato };
  enum class Volume : u8 { None, Set, Slide, Reset };
  enum class Misc : u8 { None, SetSampleOffset, PositionJump, PatternBreak, RetrigNote, SetSpeed };

  std::optional<u8> period;  ///< index into kPeriods rows
  std::optional<u8> sample;
  Tone tone = Tone::None;
  u8 toneA = 0, toneB = 0;               ///< arpeggio offsets
  std::optional<u8> portaTarget, portaSpeed, vibRate, vibDepth;  ///< speeds/rates: absent when zero
  Volume volume = Volume::None;
  int volumeArg = 0;                     ///< Set: volume, Slide: signed speed
  Misc misc = Misc::None;
  u8 miscArg = 0;
};

struct SongSample {
  std::string name;
  std::vector<u8> data;
  u8 finetune = 0, volume = 0;
  std::optional<std::pair<std::size_t, std::size_t>> repeat;  ///< start, length
};

struct Mod {
  using Row = std::array<SongNote, 4>;
  using Pattern = std::array<Row, 0x40>;

  static Mod load(ByteView data);

  std::string name;
  std::vector<SongSample> samples;  ///< index 0 is an empty placeholder
  std::vector<Pattern> patterns;
  std::vector<u8> positions;
  u8 posRestart = 0;
};

}  // namespace pfr
