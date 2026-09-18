#pragma once
// Finds the engine's data structures inside a table executable.
//
// Each of the four table programs was compiled separately, so the same structure sits at a
// different address in each binary. Rather than hard-coding one table's addresses, we locate
// each structure the way it can be identified with certainty: by its contents where the
// contents are mathematically fixed, and otherwise by the instruction sequence in the
// original code that refers to it.
#include "data/TableData.h"

namespace pfr {

struct TableLayout {
  std::size_t sineTable = 0;       ///< 2560 big-endian words of sin(i*2pi/2048)*16384
  std::size_t limits = 0;          ///< velocity clamps, bumper and slingshot kicks, nudge
  std::size_t materials = 0;       ///< eight 16-byte collision material records
  std::size_t gravityLowRes = 0;   ///< four (gx, gy) pairs for the 320x240 mode
  std::size_t gravityHighRes = 0;  ///< four (gx, gy) pairs for the 350-line mode
  std::size_t flipperRecords = 0;  ///< first of three 0x3c-byte flipper records
  std::array<u16, 3> flipperSegments{};  ///< frame block for each record, 0 when unused
  /// Layer transition zones. The first list is consulted while the ball is on the ramp
  /// layer, the second while it is on the playfield; entering a rectangle flips the layer.
  std::size_t layerZonesWhenUpper = 0;
  std::size_t layerZonesWhenLower = 0;

  bool complete() const {
    return sineTable && limits && materials && gravityHighRes && flipperRecords;
  }

  static TableLayout resolve(const TableData& table);

  /// Reads a list of 8-byte rectangles (x1, y1, x2, y2) ending with a word of -1.
  static std::vector<Rect> readZoneList(ByteView data, std::size_t offset);
};

}  // namespace pfr
