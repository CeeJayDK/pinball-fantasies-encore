#include "engine/data/PhysmapPatches.h"

namespace encore {

namespace {

/// A patch described in the data segment by a five-word record: raised shape, dropped shape,
/// position as an offset into the 40-byte-pitch collision plane, width in bytes, height in rows.
/// Each shape stores its rows with all three planes interleaved; only the solid plane (the
/// second of the three) is copied into the collision map.
PhysmapPatch readRecord(ByteView d, std::size_t record, bool overhead) {
  PhysmapPatch p;
  p.overhead = overhead;
  const std::size_t raisedAt = rd16le(d, record);
  const std::size_t droppedAt = rd16le(d, record + 2);
  const int address = rd16le(d, record + 4);
  p.byteWidth = rd16le(d, record + 6);
  p.height = rd16le(d, record + 8);
  p.byteX = address % 40;
  p.y = address / 40;
  auto shape = [&](std::size_t base) {
    Bytes rows;
    for (int y = 0; y < p.height; ++y)
      for (int bx = 0; bx < p.byteWidth; ++bx) {
        const std::size_t at = base + static_cast<std::size_t>((y * 3 + 1) * p.byteWidth + bx);
        rows.push_back(at < d.size() ? d[at] : 0);
      }
    return rows;
  };
  p.raised = shape(raisedAt);
  p.dropped = shape(droppedAt);
  return p;
}

}  // namespace

std::vector<PhysmapPatch> extractPhysmapPatches(ByteView d, int tableIndex) {
  std::vector<PhysmapPatch> out;
  if (tableIndex == 3) {
    // Same order as StonesGate.
    out.push_back(readRecord(d, 0x1265, false));
    out.push_back(readRecord(d, 0x123d, false));
    out.push_back(readRecord(d, 0x1233, false));
    out.push_back(readRecord(d, 0x1247, true));
    out.push_back(readRecord(d, 0x1251, true));
    out.push_back(readRecord(d, 0x125b, true));
  }
  return out;
}

}  // namespace encore
