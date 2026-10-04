#include "engine/data/TableLights.h"

#include <algorithm>

#include "core/Log.h"

namespace encore {
namespace {

/// Where each table keeps its array of pointers to light records, and how many it holds.
struct LightTable {
  u16 pointers;
  int count;
};
constexpr LightTable kTables[4] = {{0x12bd, 56}, {0x0fdd, 67}, {0x0d8b, 38}, {0x11d0, 44}};

/// Light colours are stored on a 0 to 95 scale rather than the palette's own range.
u8 expand(u8 value) { return static_cast<u8>((static_cast<unsigned>(value) * 0xa2) >> 6); }

}  // namespace

std::vector<TableLight> extractLights(ByteView d, int tableIndex) {
  std::vector<TableLight> out;
  const LightTable& t = kTables[std::clamp(tableIndex, 0, 3)];
  for (int i = 0; i < t.count; ++i) {
    const std::size_t pointer = t.pointers + static_cast<std::size_t>(i) * 2;
    if (pointer + 2 > d.size()) break;
    const std::size_t record = rd16le(d, pointer);
    if (record + 2 > d.size()) {
      log::warn("light " + std::to_string(i) + " points outside the data segment");
      continue;
    }
    TableLight light;
    light.baseIndex = d[record];
    const int count = d[record + 1];
    for (int c = 0; c < count; ++c) {
      const std::size_t at = record + 2 + static_cast<std::size_t>(c) * 3;
      if (at + 3 > d.size()) break;
      light.colors.push_back({expand(d[at]), expand(d[at + 1]), expand(d[at + 2])});
    }
    out.push_back(std::move(light));
  }
  return out;
}

}  // namespace encore
