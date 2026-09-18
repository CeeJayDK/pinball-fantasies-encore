#pragma once
// One flipper, decoded from a 0x3c-byte record in the table's data segment.
// The flipper's collision shape is a bitmap stamped into the wall mask, one frame per
// angle, exactly as the original did; the ball therefore collides with it as with a wall,
// and the flipper's angular velocity is added separately as a contact impulse.
#include "core/Types.h"

namespace pfr {

class Mask;

struct Flipper {
  static constexpr int kPositionPerFrame = 55;

  // --- static configuration, read from the record ---
  int keySide = 0;      ///< 2 = driven by the left key, 1 = the right key, 0 = unused slot
  int originX = 0, originY = 0;
  int frameWidth = 0;   ///< pixels
  int frameRows = 0;
  Rect box;             ///< the ball must be inside this for the flipper to matter
  int pivotX = 0, pivotY = 0;
  bool vertical = false;///< swaps the axes in the impulse formula
  int maxFrame = 0;
  int positionAtMax = 0;
  int stepUp = 0;       ///< added to the angular speed while the key is held (negative)
  int stepDown = 0;     ///< added while the key is released (positive)
  int initialUp = 0;    ///< the speed a press snaps to immediately (negative)
  /// The block holding this flipper's frames. The original writes this into the record at
  /// run time, so the value stored in the file is meaningless; TableLayout recovers it
  /// from the instruction that performs the write.
  u16 maskSegment = 0;
  int frameStack = -1;  ///< index into Physics' own reshaped frame masks

  // --- state ---
  int omega = 0;
  int position = 0;
  int frame = 0;
  int lastStamped = -1;

  bool valid() const { return keySide != 0; }
  /// Advances one physics sub-step. `held` is the state of this flipper's key.
  void update(bool held, bool enabled);
  /// Reads a record from a table's data segment.
  static Flipper decode(ByteView dataSegment, std::size_t offset);
};

}  // namespace pfr
