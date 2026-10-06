#include "engine/data/Bumpers.h"

#include <algorithm>

namespace encore {
namespace {

/// Where each table keeps its bumper list and its kicker list.
struct BumperAddresses {
  u16 bumpers, kickers;
};
constexpr BumperAddresses kTables[4] = {
    {0x0cf3, 0x0d1b}, {0x0aad, 0x0adf}, {0x0924, 0x094c}, {0x096a, 0x099c},
};

/// Ten bytes each: the rectangle, then a pointer to a record holding the sound and score.
/// A first word of 0xffff ends the list. Returns false if anything looks implausible.
bool readList(ByteView d, std::size_t at, bool kicker, std::vector<Bumper>& out) {
  const std::size_t before = out.size();
  for (; at + 10 <= d.size(); at += 10) {
    const u16 first = static_cast<u16>(rd16le(d, at));
    if (first == 0xffff) return out.size() > before;
    const i16 x1 = static_cast<i16>(first);
    const i16 y1 = static_cast<i16>(rd16le(d, at + 2));
    const i16 x2 = static_cast<i16>(rd16le(d, at + 4));
    const i16 y2 = static_cast<i16>(rd16le(d, at + 6));
    const std::size_t record = rd16le(d, at + 8);
    if (x1 < 0 || x1 > x2 || x2 > 336 || y1 < 0 || y1 > y2 || y2 > 600) return false;
    if (record + 14 > d.size()) return false;
    Bumper b;
    b.rect = {x1, y1, x2 - x1 + 1, y2 - y1 + 1};
    b.kicker = kicker;
    b.sfx = readSfxNear(d, rd16le(d, record));
    b.score = Score::fromDigits(d, record + 2);
    if (!b.sfx.valid() || b.score.isZero()) return false;
    out.push_back(b);
    if (out.size() > 32) return false;
  }
  return false;
}

/// The recorded addresses drift by a byte in some copies, so look either side.
void readListNear(ByteView d, std::size_t hint, bool kicker, std::vector<Bumper>& out) {
  for (int delta : {0, -1, 1, -2, 2}) {
    if (delta < 0 && hint < static_cast<std::size_t>(-delta)) continue;
    std::vector<Bumper> attempt = out;
    if (readList(d, hint + static_cast<std::size_t>(delta), kicker, attempt)) {
      out = std::move(attempt);
      return;
    }
  }
}

}  // namespace

std::vector<Bumper> extractBumpers(ByteView d, int tableIndex) {
  const BumperAddresses& t = kTables[std::clamp(tableIndex, 0, 3)];
  std::vector<Bumper> out;
  readListNear(d, t.bumpers, false, out);
  readListNear(d, t.kickers, true, out);
  return out;
}

}  // namespace encore
