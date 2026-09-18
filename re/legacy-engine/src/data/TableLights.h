#pragma once
// The playfield's lights.
//
// The original never draws a light. Each light owns a small run of palette entries, and
// switching it on or off simply changes those colours: lit shows them at full strength,
// unlit at half. That is why the playfield picture can stay untouched from frame to frame.
#include <vector>

#include "core/Types.h"

namespace pfr {

struct TableLight {
  u8 baseIndex = 0;          ///< first palette entry this light owns
  std::vector<Rgb> colors;   ///< its colours when lit, one per entry
};

/// Reads a table's light table out of its data segment.
std::vector<TableLight> extractLights(ByteView dataSegment, int tableIndex);

}  // namespace pfr
