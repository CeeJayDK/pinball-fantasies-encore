#pragma once
// A table as it was played, from the moment it was opened: enough to play it all again,
// with no window and no sound, and arrive at the same games with the same scores.
//
// The game is the same every time given the same start (the table, the random seed, the
// options and the table's high scores), the same keys at the same frames, and the music in
// the same state at the same frames. The music is the one thing played on the side, by the
// audio thread, at moments that depend on the sound card; so what the game saw of it at the
// start of each frame is recorded too, whenever it changed by itself (TableSequencer).
#include <optional>
#include <string>
#include <vector>

#include "assets/Bcd.h"
#include "core/Types.h"
#include "game/Config.h"

namespace pfr {

struct Replay {
  /// Raised whenever a recording would no longer play back the same: a change in the file's
  /// layout, or in how the tables play.
  static constexpr u16 kFormat = 1;

  struct Event {
    enum class Kind : u8 { KeyDown, KeyUp, Music };
    u32 frame = 0;  ///< before this frame for keys, at its start for the music
    Kind kind = Kind::KeyDown;
    u32 value = 0;  ///< a Key, or the music's state as TableSequencer packs it
    bool operator==(const Event&) const = default;
  };
  /// A game as it ended: the frame of its game over and each player's score.
  struct Game {
    u32 endFrame = 0;
    bool abandoned = false;  ///< quit from the pause menu before the end
    std::vector<Bcd> scores;
    bool operator==(const Game&) const = default;
  };

  int table = 0;
  u64 seed = 0;
  Options options;  ///< as the table opened; changed in the pause menu by keys, which are here
  HighScores highScores;  ///< the table's, which decide whether a game ends asking for a name
  u32 frames = 0;
  std::vector<Event> events;
  std::vector<Game> games;

  Bytes save() const;
  static std::optional<Replay> load(ByteView data);
};

/// Plays a recording again from the table's files, and returns what that recorded: for a
/// faithful recording, the same events, games and scores.
Replay replay(ByteView prg, ByteView module, const Replay& recording);

/// What a server will take.
struct VerifyLimits {
  u32 maxFrames = 60 * 60 * 60 * 3;  ///< three hours
  std::size_t maxEvents = 2'000'000;
};

/// What the server makes of a recording: one it can play, played again. The games and scores
/// are the ones the replay arrives at, not the ones the recording claims.
struct Verdict {
  bool ok = false;
  std::string reason;  ///< why not, when not
  Replay replayed;     ///< as played again: its games and scores are the ones to go by
  bool claimsMatch = false;  ///< the recording's own games and scores came out the same
};
Verdict verify(ByteView prg, ByteView module, const Replay& recording, const VerifyLimits& limits = {});

}  // namespace pfr
