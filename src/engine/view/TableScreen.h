#pragma once
// The table as the player sees it, drawn from what the engine knows: the window on the
// playfield's picture, the ball, the flippers, the plunger, and the display's dots below.
// The original draws all of this into the video card's memory as it goes; the engine does not
// draw, so the picture is made here, once a frame, from where things are.
#include "engine/data/BallSprite.h"
#include "engine/data/TableData.h"
#include "engine/table/Engine.h"

namespace encore {

class TableScreen {
 public:
  static constexpr int kWidth = 320;
  static constexpr int kDisplayRows = 33;

  TableScreen(const std::filesystem::path& prg, int table);

  /// The screen's rows in the mode the options chose: 240, or 350.
  static int height(bool highResolution) { return highResolution ? 350 : 240; }
  /// One frame: `height` rows of 320 colours of the engine's palette (Engine::colours).
  void draw(Engine& engine, u8* frame, int height) const;
  /// Follows a table from its start: the flippers are drawn into the playfield's picture as
  /// the engine says they move. The engine must outlive this, or be let go with detach().
  void attach(Engine& engine);

  const TableData& data() const { return data_; }

 private:
  void flipperDrawn(Engine& engine, u16 record, u16 was, u16 now);

  TableData data_;
  BallSprite ball_;
  Bytes picture_;  ///< the playfield as it now is on the screen: its picture, and the flippers as they stand
};

}  // namespace encore
