#include "engine/data/Triggers.h"

#include <algorithm>
#include <optional>

namespace encore {
namespace {

/// Where each table keeps its trigger lists.
struct TriggerAddresses {
  u16 hit;
  u16 rollGround, rollOverhead, rollGroundTilted, rollOverheadTilted;
};
constexpr TriggerAddresses kTables[4] = {
    {0x0d71, 0x0d9b, 0x0e29, 0x0ea3, 0x0ec3},
    {0x0b51, 0x0b91, 0x0c29, 0x0c53, 0x0c53},
    {0x09a2, 0x09e0, 0x0a78, 0x0aca, 0x0ad6},
    {0x0a00, 0x0a5e, 0x0b14, 0x0bac, 0x0bcc},
};

/// The framebuffer is wider than the visible screen, and a few rectangles use that margin.
constexpr int kMaxX = 336;
constexpr int kMaxY = 600;

/// Ten bytes per entry: four signed words of rectangle, then the handler address. A zero
/// first word ends the list. Returns nothing if anything in the list looks implausible.
std::optional<std::vector<TriggerArea>> readList(ByteView d, std::size_t offset) {
  std::vector<TriggerArea> out;
  for (std::size_t at = offset; at + 10 <= d.size(); at += 10) {
    if (rd16le(d, at) == 0) return out;
    const i16 x1 = static_cast<i16>(rd16le(d, at));
    const i16 y1 = static_cast<i16>(rd16le(d, at + 2));
    const i16 x2 = static_cast<i16>(rd16le(d, at + 4));
    const i16 y2 = static_cast<i16>(rd16le(d, at + 6));
    const u16 handler = static_cast<u16>(rd16le(d, at + 8));
    const bool sane = x1 >= 0 && x1 <= x2 && x2 <= kMaxX && y1 >= 0 && y1 <= y2 && y2 <= kMaxY && handler != 0;
    if (!sane) return std::nullopt;
    out.push_back({{x1, y1, x2 - x1 + 1, y2 - y1 + 1}, handler});
    if (out.size() > 64) return std::nullopt;
  }
  return std::nullopt;
}

/// The recorded addresses are not always exact, so look either side of the hint and take
/// the longest list that reads cleanly.
std::vector<TriggerArea> readListNear(ByteView d, std::size_t hint) {
  std::vector<TriggerArea> best;
  for (int delta = -2; delta <= 2; ++delta) {
    const std::size_t at = hint + static_cast<std::size_t>(delta);
    if (delta < 0 && hint < static_cast<std::size_t>(-delta)) continue;
    if (auto list = readList(d, at); list && list->size() > best.size()) best = std::move(*list);
  }
  return best;
}

}  // namespace

TableTriggers extractTriggers(ByteView d, int tableIndex) {
  const TriggerAddresses& t = kTables[std::clamp(tableIndex, 0, 3)];
  TableTriggers out;
  out.hit = readListNear(d, t.hit);
  out.rollGround = readListNear(d, t.rollGround);
  out.rollOverhead = readListNear(d, t.rollOverhead);
  out.rollGroundTilted = readListNear(d, t.rollGroundTilted);
  out.rollOverheadTilted = readListNear(d, t.rollOverheadTilted);
  return out;
}

}  // namespace encore
