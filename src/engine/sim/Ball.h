#pragma once
// The ball, kept in the original's integer representation so the simulation reproduces
// the 1994 behaviour exactly, including its truncation and wrapping.
#include "core/Types.h"

namespace encore {

struct Ball {
  static constexpr int kRadius = 8;
  static constexpr int kCentreOffset = 7;  ///< collision centre relative to the sprite corner
  static constexpr int kSpriteSize = 16;
  static constexpr int kDrawOffsetY = 0x21;

  i32 xFixed = 0;  ///< 22.10 fixed point
  i32 yFixed = 0;
  i16 x = 0, y = 0;   ///< integer position of the sprite's top-left corner
  i16 vx = 0, vy = 0; ///< 1/1024 pixel per physics sub-step
  i16 spin = 0;       ///< surface speed, same units
  bool upper = false; ///< true while the ball is on the ramp layer
  bool active = false;///< false while the ball is hidden (serve, drain, locks)
  bool lost = false;  ///< set when the ball falls past the bottom of the table

  void place(int px, int py);
  void setVelocity(int nvx, int nvy);
  int centreX() const { return x + kCentreOffset; }
  int centreY() const { return y + kCentreOffset; }
};

}  // namespace encore
