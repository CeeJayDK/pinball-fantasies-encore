#include "data/FlipperGraphics.h"

#include <algorithm>

#include "core/Log.h"
#include "data/TableData.h"

namespace pfr {
namespace {

/// Per-table facts that cannot be derived: where the flipper records sit, how many
/// four-pixel groups the shared lookup table holds, and how many flippers there are.
struct TableAddresses {
  u16 records;
  int pixelCount;
  int flippers;
};
constexpr TableAddresses kTables[4] = {
    {0x6950, 0x06d4 / 4, 3},
    {0x6940, 0x00a8 / 4, 3},
    {0x66d0, 0x00dc / 4, 3},
    {0x7360, 0x0078 / 4, 2},
};

/// The copy lists address video memory, whose rows are 84 bytes of four pixels each.
constexpr int kVramPitch = 0x54;
/// The copy lists name pixel groups by their address, which starts at this value.
constexpr u16 kPixelTableBase = 0xd4f4;
/// Each copy list begins with a count and eight more words of bookkeeping.
constexpr int kCopyListHeader = 0x12;

}  // namespace

std::vector<FlipperFrames> extractFlipperGraphics(const TableData& table) {
  std::vector<FlipperFrames> out;
  const TableAddresses& t = kTables[std::clamp(table.index, 0, 3)];
  const ByteView d = table.dataSegment;
  const Bytes& playfield = table.playfield;
  // The copy lists and the four-pixel groups are addressed from the start of their own
  // blocks, so the blocks themselves are the right base.
  const ByteView copySegment = table.frameTable;
  const ByteView pixelSegment = table.smallImageA;
  if (copySegment.empty() || pixelSegment.empty()) return out;

  for (int index = 0; index < t.flippers; ++index) {
    const std::size_t record = t.records + static_cast<std::size_t>(index) * 0x3c;
    if (record + 0x3c > d.size()) break;
    auto word = [&](std::size_t at) { return rd16le(d, record + at); };

    FlipperFrames f;
    f.width = word(0x06) * 16;
    f.height = word(0x08);
    f.x = word(0x02);
    f.y = word(0x04);
    const int steps = word(0x20);
    const std::size_t copyList = word(0x36) + static_cast<std::size_t>(index) * 8;
    const std::size_t copyStride = word(0x38);
    if (f.width <= 0 || f.height <= 0 || f.x < 0 || f.y < 0) continue;
    if (f.x + f.width > TableData::kWidth || f.y + f.height > TableData::kHeight) continue;

    // Frame zero is the playfield with no flipper drawn on it.
    Bytes frame(static_cast<std::size_t>(f.width) * f.height);
    for (int row = 0; row < f.height; ++row)
      std::copy_n(playfield.begin() + static_cast<std::size_t>(f.y + row) * TableData::kWidth + f.x, f.width,
                  frame.begin() + static_cast<std::size_t>(row) * f.width);
    f.frames.push_back(frame);

    // Each further frame is the previous one with a list of four-pixel groups copied in.
    for (int step = 0; step < steps; ++step) {
      const std::size_t list = copyList + copyStride * static_cast<std::size_t>(step);
      if (list + kCopyListHeader > copySegment.size()) break;
      const int count = rd16le(copySegment, list);
      for (int i = 0; i < count; ++i) {
        const std::size_t entry = list + kCopyListHeader + static_cast<std::size_t>(i) * 4;
        if (entry + 4 > copySegment.size()) break;
        const u16 destination = rd16le(copySegment, entry);
        const u16 source = rd16le(copySegment, entry + 2);
        if (source < kPixelTableBase) continue;
        const std::size_t group = source - kPixelTableBase;
        const int dx = (destination % kVramPitch) * 4;
        const int dy = destination / kVramPitch;
        if (dx < 0 || dy < 0 || dx + 4 > f.width || dy >= f.height) continue;
        // The four pixels of a group are stored one video plane at a time.
        for (int plane = 0; plane < 4; ++plane) {
          const std::size_t at = group + static_cast<std::size_t>(t.pixelCount) * plane;
          if (at >= pixelSegment.size()) continue;
          frame[static_cast<std::size_t>(dy) * f.width + dx + plane] = pixelSegment[at];
        }
      }
      f.frames.push_back(frame);
    }
    out.push_back(std::move(f));
  }
  return out;
}

}  // namespace pfr
