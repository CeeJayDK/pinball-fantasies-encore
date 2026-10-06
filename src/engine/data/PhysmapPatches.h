#pragma once
// Gates and drop targets: rectangles of the solid collision plane that the table's rules
// swap between a "raised" and a "dropped" shape while the game runs.
#include <vector>

#include "core/Types.h"

namespace encore {

struct PhysmapPatch {
  bool overhead = false;  ///< which layer's solid plane is patched
  int byteX = 0;          ///< left edge in bytes (8 pixels each)
  int y = 0;
  int byteWidth = 0;
  int height = 0;
  Bytes raised;   ///< packed rows, byteWidth bytes each
  Bytes dropped;
};

/// Stones 'n Bones gate names, in the order extractPhysmapPatches returns them for table 4.
enum class StonesGate { Kickback, TowerEntry, RampTower, RampLeft0, RampLeft1, RampLeft2 };

/// The table's patches, indexed by the per-table gate enum (empty where not yet mapped).
std::vector<PhysmapPatch> extractPhysmapPatches(ByteView dataSegment, int tableIndex);

}  // namespace encore
