#pragma once
// A table as the game plays it: the table's program written again (engine/table), the sound
// driver it talks to (engine/audio) and the picture of it (engine/view), put together and
// given keys. Everything that happens on it follows from how it was set up and from the keys
// it was given at each frame, so a game played on it can be played again (Recording.h).
//
// It also does what this version adds to a table: the options changed while the game is
// paused, the steeper angle, the whole table on one screen, the lamps all lit or all out to
// look at the artwork, and the question whether a best score is to be sent online.
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "core/Keys.h"
#include "engine/audio/MusicDriver.h"
#include "engine/game/Recording.h"
#include "engine/table/Engine.h"
#include "engine/view/TableScreen.h"
#include "game/Config.h"

namespace encore {

class TableGame {
 public:
  struct Setup {
    Options options;
    HighScores highScores{};
    /// Where the table's source of chance begins. The original counts the turns of its own
    /// loop, which depend on the machine; here the count goes on evenly, from this.
    u16 chance = 0;
    Recording::Carry carry;
    bool picture = true;  ///< false for a table nobody will look at
  };

  /// `prg` and `module` are the bytes of TABLEn.PRG and TABLEn.MOD; `table` 0 to 3.
  TableGame(ByteView prg, ByteView module, int table, const Setup& setup);
  ~TableGame();

  int table() const { return engine_->table(); }
  void key(Key key, bool down);
  /// One frame of the game: a sixtieth of a second of it, and of its music.
  void frame();
  u32 frames() const { return frames_; }

  /// No game is being played: the table waits for one.
  bool waiting() const { return engine_->B(0x3713) == 0xff; }
  /// Whether this key, pressed now, would start a game.
  bool startsGame(Key key) const;
  bool paused() const { return engine_->isPaused(); }
  /// The table was left: back to the menu.
  bool left() const { return engine_->exited() || !failure_.empty(); }
  /// What went wrong, if the table stopped because something did.
  const std::string& failure() const { return failure_; }
  int players() const { return engine_->B(0x3716); }
  int player() const { return engine_->B(0x371a); }
  int ball() const { return engine_->B(0x33dc); }
  Bcd score(int player) const;

  /// The options as they now are, the ones changed while paused among them.
  Options options() const;
  HighScores highScores() const;
  /// True once after each change, for whoever keeps them.
  bool optionsChanged();
  bool highScoresChanged();
  /// What a game started from this table takes over from it.
  Recording::Carry carryOver() const;

  /// Everything since this table was made: for playing it again.
  const Recording& recording() const { return recording_; }
  /// The first player, asked after typing initials for a best score, wants the game sent.
  bool sendOnline() const { return sendOnline_; }
  bool askingOnline() const { return asking_; }
  /// Initials are being typed for a best score.
  bool askingName() const;

  /// The screen: 320 across and this many rows, each dot one of 256 colours.
  int screenHeight() const;
  /// `hd`: also what the pictures drawn again at high resolution need (gfx/HdLayer.h).
  void draw(u8* frame, Rgb* colours, HdFrame* hd = nullptr) const;
  std::vector<Cutout> flipperPictures() const { return screen_ ? screen_->flipperPictures(*engine_) : std::vector<Cutout>{}; }
  std::vector<bool> flipperIsLeft() const { return screen_ ? screen_->flipperIsLeft(*engine_) : std::vector<bool>{}; }
  Cutout ballPicture() const { return screen_ ? screen_->ballPicture(*engine_) : Cutout{}; }
  bool ballTrail = true;
  /// The lamps as the game has them (0), all lit (1) or all out (2): for looking at the artwork.
  void showLamps(int how) { lamps_ = how; }

  /// The sound made since it was last asked for, for the sound card (48000 a second, left and
  /// right); from any thread.
  void sound(float* out, int frames);
  /// With nobody listening, what was made is let go of instead.
  void noSound() { music_.discard(); }
  bool silent = false;  ///< plays, but hands out silence

  Engine& engine() { return *engine_; }
  MusicDriver& music() { return music_; }

 private:
  int viewRows() const;
  int viewTop() const;
  void follow();
  void pausedKey(Key key);
  void note();

  Options options_;
  bool engineHigh_ = true;  ///< the screen mode the table was started for, which its speeds are for
  MusicDriver music_;
  std::unique_ptr<Engine> engine_;
  std::unique_ptr<TableScreen> screen_;
  u32 frames_ = 0;
  bool playing_ = false;
  HighScores bestAtStart_{};
  Recording recording_;
  std::string failure_;
  // what this version adds
  Options saved_;             ///< as last told to whoever keeps them
  HighScores savedScores_{};
  bool optionsChanged_ = false, scoresChanged_ = false;
  bool asking_ = false, sendOnline_ = false;
  u16 lastWait_ = 0;          ///< the display's wait a frame ago, to see the initials' end come
  int lamps_ = 0;             ///< 0 as the game has them, 1 all lit, 2 all out
  int manual_ = 0;            ///< rows the screen was moved by hand while paused
  bool up_ = false, down_ = false;
  i32 camera_ = 0;            ///< where the screen looks, in sixteenths of a row, when not where the table's own would
};

}  // namespace encore
