#pragma once
// One game as it was played, from the key that started it to its game over: enough to play
// it again, with no window and no sound, and arrive at the same score.
//
// Every game is played on a table of its own, made afresh when the game is started, so it
// depends on nothing played before it. It is the same every time given the same start (the
// table, where its source of chance began, the options, the table's high scores and what it
// took over from the table it was started on) and the same keys
// at the same frames. The music is played by the game's own time (engine/audio/MusicDriver.h),
// so what it does to the game is the same every time too.
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/Bcd.h"
#include "core/Keys.h"
#include "core/Types.h"
#include "game/Config.h"

namespace encore {

struct Recording {
  /// The tables as recordings are named after them, eight letters each.
  static constexpr const char* kTableCodes[4] = {"PARTYLND", "SPDDEVLS", "GAMESHOW", "STONBONE"};

  /// Raised whenever a recording would no longer play back the same: a change in the file's
  /// layout, or in how the tables play.
  static constexpr u16 kFormat = 4;

  struct Event {
    u32 frame = 0;  ///< the key is pressed or let go before this frame
    bool down = true;
    Key key = Key::None;
    bool operator==(const Event&) const = default;
  };
  /// What a game takes over from the table it was started on, beyond the options: cheats
  /// typed while that table waited, and where its screen was looking.
  struct Carry {
    bool noTilt = false;
    bool otherSteps = false;  ///< the cheat that has the ball move at the other screen mode's pace
    u8 balls = 0;  ///< as the options or the balls cheat have it; 0: as the options have it
    u16 scrollPos = 0xffff;  ///< where the waiting table's screen had drifted to; 0xffff: where a table opens
    u16 scrollAt = 0;        ///< and where it was looking, in sixteenths of a row
    bool operator==(const Carry&) const = default;
  };
  /// The game as it ended: the frame of its game over and each player's score.
  struct Game {
    u32 endFrame = 0;
    bool abandoned = false;  ///< quit from the pause menu before the end
    std::vector<Bcd> scores;
    std::array<u8, 3> initials{};  ///< typed for a high score by the first player; zeros if none
    bool operator==(const Game&) const = default;
  };

  int table = 0;
  u16 chance = 0;  ///< where the table's source of chance begins
  Options options;  ///< as the table opened; changed in the pause menu by keys, which are here
  HighScores highScores;  ///< the table's, which decide whether a game ends asking for a name
  Carry carry;
  u32 frames = 0;
  std::vector<Event> events;
  std::vector<Game> games;  ///< the one game, once it is over

  /// Played with a cheat that makes it easier: no tilt, another pace, or more balls than the
  /// options give. (Fewer, as "fair play" can leave, is only harder.)
  bool cheated() const { return carry.noTilt || carry.otherSteps || carry.balls > options.balls; }

  Bytes save() const;
  static std::optional<Recording> load(ByteView data);
  /// What a recording is called: FANTASY-<table>-<initials>-[<tag>-]<score>-<when>.RPL, as
  /// FANTASY-STONBONE-RDX-67108120-20261002-2153.RPL. Initials not typed are ---, a space in
  /// them is _, and the score is the first player's. `tag` is the server's, for its own.
  std::string fileName(const std::string& when, const std::string& tag = {}) const;
};

/// Plays a recording again from the table's files, and returns what that recorded: for a
/// faithful recording, the same events, game and score.
Recording replay(ByteView prg, ByteView module, const Recording& recording);

/// What a server will take.
struct VerifyLimits {
  u32 maxFrames = 60 * 60 * 60 * 3;  ///< three hours
  std::size_t maxEvents = 2'000'000;
};

/// What a server makes of a recording: one whole game, played to its end without cheats, that
/// it can play again. The game and its score are the ones the replay arrives at, not the ones
/// the recording claims.
struct Verdict {
  bool ok = false;
  std::string reason;  ///< why not, when not
  Recording replayed;     ///< as played again: its games and scores are the ones to go by
  bool claimsMatch = false;  ///< the recording's own games and scores came out the same
};
Verdict verify(ByteView prg, ByteView module, const Recording& recording, const VerifyLimits& limits = {});

}  // namespace encore
