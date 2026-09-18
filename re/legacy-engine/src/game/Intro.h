#pragma once
// The opening sequence: three publisher and developer logos, a title card, and the wide
// game logo, paced the way the original paced them against its music.
#include "data/IntroData.h"
#include "gfx/Framebuffer.h"
#include "gfx/Palette.h"

namespace pfr {

class Intro {
 public:
  void init(const IntroData& data);
  /// `skip` ends the sequence early, as the space bar did in the original.
  void update(bool skip, double dt);
  void render(Framebuffer& frame, Palette& palette) const;

  bool finished() const { return finished_; }
  int width() const;
  int height() const;

 private:
  struct Stage {
    int screen = -1;     ///< index into IntroData::introScreens, or -1
    int picture = -1;    ///< a whole picture instead, or -1
    double end = 0;      ///< when this stage gives way to the next, in seconds
    double fadeIn = 0;
    double fadeOut = 0;
  };

  double brightness() const;
  const Stage& stage() const { return stages_[static_cast<std::size_t>(current_)]; }

  const IntroData* data_ = nullptr;
  std::vector<Stage> stages_;
  int current_ = 0;
  double elapsed_ = 0;    ///< time within the whole sequence
  bool finished_ = false;
};

}  // namespace pfr
