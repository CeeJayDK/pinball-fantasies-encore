#pragma once
// The in-game screen: a scrolling window onto the playfield with the dot-matrix display
// across the bottom, and the ball simulated by the physics engine.
//
// The display sits at the top of video memory in the original, which is where its data is
// found, but a hardware line-compare puts it at the bottom of the screen. Putting the
// display at the bottom is in fact why the original used that trick at all.
#include "data/BallSprite.h"
#include "data/FlipperGraphics.h"
#include "data/DmFont.h"
#include "data/Bumpers.h"
#include "data/PhysmapPatches.h"
#include "data/TableSound.h"
#include "game/Score.h"
#include "data/Triggers.h"
#include "audio/ModPlayer.h"
#include "data/TableLights.h"
#include "game/Dmd.h"
#include "data/TableData.h"
#include "gfx/Framebuffer.h"
#include "gfx/Palette.h"
#include "sim/Ball.h"
#include "sim/Physics.h"

namespace pfr {

/// What the play screen needs from the keyboard, keeping the core free of platform types.
struct PlayInput {
  bool leftFlipper = false;
  bool rightFlipper = false;
  bool nudge = false;
  bool plunger = false;   ///< held to draw the plunger back, released to launch
  bool serve = false;     ///< put a ball into the shooter lane
};

class Play {
 public:
  static constexpr int kWidth = 320;
  static constexpr int kDisplayRows = 33;   ///< the dot-matrix area, drawn at the bottom
  static constexpr int kViewRowsHigh = 317;    ///< playfield rows visible in the 350-line mode
  static constexpr int kViewRowsNormal = 207;  ///< and in the 320x240 mode
  static constexpr int kPlungerWidth = 10;
  static constexpr int kPlungerHeight = 23;
  static constexpr int kHeightHigh = kDisplayRows + kViewRowsHigh;
  static constexpr int kHeightNormal = kDisplayRows + kViewRowsNormal;

  int viewRows() const { return highResolution_ ? kViewRowsHigh : kViewRowsNormal; }
  int height() const { return kDisplayRows + viewRows(); }

  void init(const TableData& table, bool highResolution);
  /// The player the table's sound effects and music cues are sent to.
  void setSound(ModPlayer* player) { player_ = player; }
  /// Advances one video frame: the physics runs in sub-steps inside this.
  void update(const PlayInput& input, double dt);
  void render(Framebuffer& frame) const;
  /// Rebuilds the palette for this frame: the table's own colours, with every light shown
  /// at full strength when lit and half when not.
  void applyPalette(Palette& palette) const;

  /// Something the ball did this frame, named by the original's handler address.
  struct TriggerEvent {
    bool rolled = false;   ///< false means it was a collision
    u16 handler = 0;
  };
  const std::vector<TriggerEvent>& events() const { return events_; }
  const TableTriggers& triggers() const { return triggers_; }
  const Score& score() const { return score_; }
  const std::vector<Bumper>& bumpers() const { return bumpers_; }

  void setLight(std::size_t index, bool lit);
  bool lightLit(std::size_t index) const;
  std::size_t lightCount() const { return lights_.size(); }

  const Ball& ball() const { return ball_; }
  int cameraRow() const { return cameraRow_; }
  Physics& physics() { return physics_; }
  Dmd& display() { return dmd_; }
  void serveBall();

 private:
  void updateCamera();
  void collectTriggers();
  void applyLayerHandler(u16 handler);
  void refreshDisplay();
  void playSfx(const Sfx& sfx) const;
  void playMusic(const Jingle& jingle) const;

  const TableData* table_ = nullptr;
  BallSprite ballSprite_;
  std::vector<FlipperFrames> flipperGraphics_;
  DmAssets dmAssets_;
  Dmd dmd_;
  TableTriggers triggers_;
  std::vector<Bumper> bumpers_;
  std::vector<PhysmapPatch> patches_;
  Score score_;
  bool scoreChanged_ = true;
  TableSounds sounds_;
  ModPlayer* player_ = nullptr;
  bool leftWasHeld_ = false, rightWasHeld_ = false;
  bool launched_ = false;
  std::vector<TriggerEvent> events_;
  u16 insideRoll_ = 0;
  int tableIndex_ = 0;
  std::vector<TableLight> lights_;
  std::vector<bool> lit_;
  Physics physics_;
  Ball ball_;
  int cameraRow_ = TableData::kHeight - kViewRowsHigh;
  int cameraFixed_ = (TableData::kHeight - kViewRowsHigh) << 4;  ///< camera row in 1/16ths
  int plungerPull_ = 0;
  bool plungerWasHeld_ = false;
  u32 random_ = 0x1234;
  int scrollSpeed_ = 11;   ///< from the configuration: 20 hard, 11 medium, 9 soft
  bool highResolution_ = true;
};

}  // namespace pfr
