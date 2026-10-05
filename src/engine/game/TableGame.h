#pragma once
// A table as the game plays it: the table's program written again (engine/table), the sound
// driver it talks to (engine/audio) and the picture of it (engine/view), put together and
// given keys. Everything that happens on it follows from how it was started and from the keys
// it was given at each frame, so a game played on it can be played again.
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/Keys.h"
#include "engine/audio/MusicDriver.h"
#include "engine/table/Engine.h"
#include "engine/view/TableScreen.h"

namespace encore {

class TableGame {
 public:
  /// A table's best scores as TABLEn.HI has them: four of twelve digits, three letters and a
  /// byte of nothing.
  using BestScores = std::array<u8, 0x40>;
  /// A score: twelve digits, the highest first.
  using Score = std::array<u8, 12>;

  struct Start {
    Engine::Options options;
    BestScores bestScores{};
    /// Where the table's source of chance begins. The original counts the turns of its own
    /// loop, which depend on the machine; here the count goes on evenly, from this.
    u16 chance = 0;
  };

  /// `prg` is where TABLEn.PRG is, `module` the bytes of TABLEn.MOD; `table` 0 to 3.
  TableGame(const std::filesystem::path& prg, ByteView module, int table, const Start& start);
  ~TableGame();

  int table() const { return engine_->table(); }
  void key(Key key, bool down);
  /// One frame of the game: a sixtieth of a second of it, and of its music.
  void frame();
  u32 frames() const { return frames_; }

  /// No game is being played: the table waits for one.
  bool waiting() const { return engine_->B(0x3713) == 0xff; }
  /// The table was left: back to the menu.
  bool left() const { return engine_->exited() || !failure_.empty(); }
  /// What went wrong, if the table stopped because something did.
  const std::string& failure() const { return failure_; }
  int players() const { return engine_->B(0x3716); }
  int player() const { return engine_->B(0x371a); }
  int ball() const { return engine_->B(0x33dc); }
  Score score(int player) const;
  BestScores bestScores() const;

  /// A game that has ended on this table.
  struct Ended {
    u32 frame = 0;
    bool abandoned = false;        ///< given up before its last ball
    std::vector<Score> scores;     ///< each player's
    std::array<u8, 3> initials{};  ///< typed by the first player for a best score, or zeros
  };
  const std::vector<Ended>& ended() const { return ended_; }

  /// The screen: 320 across and this many rows, each dot one of 256 colours.
  int screenHeight() const { return TableScreen::height(options_.highResolution); }
  void draw(u8* frame, Rgb* colours) const;

  /// The sound made since it was last asked for, for the sound card (48000 a second, left and
  /// right); from any thread.
  void sound(float* out, int frames) { music_.render(out, frames); }
  /// With nobody listening, what was made is let go of instead.
  void noSound() { music_.discard(); }

  Engine& engine() { return *engine_; }
  MusicDriver& music() { return music_; }

 private:
  Engine::Options options_;
  MusicDriver music_;
  std::unique_ptr<Engine> engine_;
  TableScreen screen_;
  u32 frames_ = 0;
  bool playing_ = false;
  BestScores bestAtStart_{};
  std::vector<Ended> ended_;
  std::string failure_;
};

}  // namespace encore
