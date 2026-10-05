#pragma once
// The table as the player sees it, drawn from what the engine knows: the window on the
// playfield's picture, the ball, the flippers, the plunger, and the display's dots below.
// The original draws all of this into the video card's memory as it goes; the engine does not
// draw, so the picture is made here, once a frame, from where things are.
//
// For the pictures drawn again at high resolution (gfx/HdLayer.h) it also says, dot by dot,
// which dot of the playfield's picture each is and how lit, and where the flippers and the
// ball are, as pictures to be turned and placed.
#include "engine/data/BallSprite.h"
#include "engine/data/TableData.h"
#include "engine/table/Engine.h"
#include "gfx/HdLayer.h"

namespace encore {

/// A picture cut from the original's, to be drawn at any size: red, green, blue and how solid.
struct Cutout {
  int width = 0, height = 0;
  Bytes rgba;
};

class TableScreen {
 public:
  static constexpr int kWidth = 320;
  static constexpr int kDisplayRows = 33;

  TableScreen(const std::filesystem::path& prg, int table);
  TableScreen(Bytes prg, int table);

  /// The screen's rows in the mode the options chose: 240, or 350.
  static int height(bool highResolution) { return highResolution ? 350 : 240; }

  /// What is to be seen of the table.
  struct View {
    int height = 350;  ///< the screen's rows: the display's 33, and above them the playfield's
    int top = 0;       ///< the row of the playfield's picture at the top of the screen
    /// 0: the lights as the game has them; 1: every one lit; 2: every one out.
    int lamps = 0;
    bool ballTrail = true;
  };
  /// One frame: `height` rows of 320 colours of the palette (see colours()). With `hd`, also
  /// what the high-resolution pictures need; the ball is then left out of the frame, to be
  /// drawn as a picture of its own.
  void draw(Engine& engine, u8* frame, const View& view, HdFrame* hd = nullptr) const;
  /// The same, looking where the original would.
  void draw(Engine& engine, u8* frame, int height) const {
    draw(engine, frame, View{height, static_cast<i16>(engine.screenRow()) - kDisplayRows});
  }
  /// The 256 colours the frame is in.
  void colours(Engine& engine, Rgb* out, int lamps = 0) const;

  /// Follows a table from before its start: the flippers are drawn into the playfield's
  /// picture as the engine says they move. The engine must outlive this.
  void attach(Engine& engine);

  /// Once the table has started: the maps of what hides the ball are then where the table
  /// keeps them. Needed for the high-resolution pictures only.
  void started(Engine& engine);

  /// Each flipper as it lies at rest, cut out of the playfield's picture; and whether it is
  /// one of the left key's.
  std::vector<Cutout> flipperPictures(Engine& engine) const;
  std::vector<bool> flipperIsLeft(Engine& engine) const;
  Cutout ballPicture(Engine& engine) const;

  const TableData& data() const { return data_; }

 private:
  void flipperDrawn(Engine& engine, u16 record, u16 was, u16 now);
  void lightAmounts(Engine& engine, int lamps, std::array<u8, 256>& amount) const;

  TableData data_;
  BallSprite ball_;
  Bytes picture_;  ///< the playfield as it now is on the screen: its picture, and the flippers as they stand
  std::array<Bytes, 2> cover_;      ///< how much of the ball each dot hides, on the playfield and on the ramps
  std::vector<Bytes> flipperRest_;  ///< each flipper's shape at rest, a byte to a dot of its rectangle
};

}  // namespace encore
