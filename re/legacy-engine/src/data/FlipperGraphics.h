#pragma once
// Flipper frames, rebuilt from the original's difference encoding.
//
// The original never stores a flipper picture. It stores, for each step of the flipper's
// travel, a list of four-pixel groups to copy over the previous frame, with the pixels
// themselves held in a shared lookup table. Replaying those lists from the bare playfield
// reconstructs every frame.
#include <vector>

#include "data/MzImage.h"

namespace pfr {

struct FlipperFrames {
  int x = 0, y = 0;        ///< where the flipper sits on the playfield
  int width = 0, height = 0;
  std::vector<Bytes> frames;  ///< frame 0 is the bare playfield, then one per step

  bool valid() const { return !frames.empty() && width > 0 && height > 0; }
};

struct TableData;

/// Rebuilds the frames for every flipper of a table.
std::vector<FlipperFrames> extractFlipperGraphics(const TableData& table);

}  // namespace pfr
