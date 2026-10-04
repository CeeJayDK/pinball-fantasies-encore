#pragma once
// The front-end screen: the table chooser the original shows between games.
#include <array>
#include <vector>

#include "data/GameFiles.h"
#include "engine/data/HiScoreFile.h"
#include "engine/data/IntroData.h"
#include "gfx/Framebuffer.h"

namespace encore {

/// What the menu needs from the keyboard, so the core stays free of any platform types.
struct MenuInput {
  int functionKey = 0;   ///< 1..8 pressed this frame, else 0
  bool escape = false;
  bool page = false;     ///< space or enter pressed this frame
  bool scrollUp = false;
  bool scrollDown = false;
};

class Menu {
 public:
  static constexpr int kWidth = 640;
  static constexpr int kHeight = 480;

  void init(const GameFiles& files, const IntroData& data);
  void update(const MenuInput& input, double dt);
  /// Draws the visible window and fills `rowPalettes` with 256 colours for each of its rows.
  void render(Framebuffer& frame, std::vector<Rgb>& rowPalettes) const;

  /// 1 to 4 once the player has chosen a table, otherwise 0.
  int chosenTable() const { return chosen_; }
  void clearChoice() { chosen_ = 0; }
  bool quitRequested() const { return quit_; }
  int scrollRow() const { return scroll_; }
  const std::array<HiScoreTable, 4>& hiScores() const { return hiScores_; }

 private:
  const IntroData* data_ = nullptr;
  std::array<HiScoreTable, 4> hiScores_{};
  int scroll_ = 0;
  int targetScroll_ = 0;
  int viewOrigin_ = 0;  ///< first row of the menu proper, below the off-screen source strips
  int chosen_ = 0;
  bool quit_ = false;
  double pageTimer_ = 0;
};

}  // namespace encore
