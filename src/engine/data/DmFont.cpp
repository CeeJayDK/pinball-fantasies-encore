#include "engine/data/DmFont.h"

#include <algorithm>

namespace encore {
namespace {

/// The characters the fonts hold, in the order they are stored.
constexpr char kCharacters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ?()-";
constexpr int kGlyphCount = 40;

constexpr int kHeights[4] = {5, 8, 11, 13};

/// Where each table keeps each font, and the palette entries its dots use.
struct DmAddresses {
  u16 fonts[4];     ///< in height order: 5, 8, 11, 13
  u16 lightTable;   ///< the light pointer array, which the two colours sit either side of
  int lightCount;
  u8 indexOff, indexOn;
};
constexpr DmAddresses kTables[4] = {
    {{0x6710, 0x65d0, 0x6410, 0x6200}, 0x12bd, 56, 0x60, 0xf2},
    {{0x67a0, 0x6660, 0x64a0, 0x6290}, 0x0fdd, 67, 0x62, 0x80},
    {{0x5ff0, 0x5eb0, 0x5cf0, 0x5ae0}, 0x0d8b, 38, 0x72, 0x99},
    {{0x6d00, 0x6bc0, 0x6a00, 0x67f0}, 0x11d0, 44, 0xe7, 0x4f},
};

/// Light and dot colours are stored on a 0 to 95 scale.
u8 expandLight(u8 v) { return static_cast<u8>((static_cast<unsigned>(v) * 0xa2) >> 6); }
/// The unlit colour is an ordinary six-bit palette value instead.
u8 expandSix(u8 v) { return static_cast<u8>((v << 2) | (v >> 4)); }

}  // namespace

DmAssets extractDmAssets(ByteView d, int tableIndex) {
  DmAssets out;
  const DmAddresses& t = kTables[std::clamp(tableIndex, 0, 3)];

  for (int f = 0; f < 4; ++f) {
    DmFont& font = out.fonts[static_cast<std::size_t>(f)];
    font.height = kHeights[f];
    for (int c = 0; c < kGlyphCount; ++c) {
      const std::size_t at = t.fonts[f] + static_cast<std::size_t>(c) * font.height;
      if (at + font.height > d.size()) break;
      std::vector<u8> rows(d.begin() + at, d.begin() + at + font.height);
      font.glyphs[kCharacters[c]] = std::move(rows);
    }
    font.glyphs[' '] = std::vector<u8>(static_cast<std::size_t>(font.height), 0);
  }

  out.palette.indexOff = t.indexOff;
  out.palette.indexOn = t.indexOn;
  // The lit colour is recorded just before the light pointer array, the unlit one just after.
  const std::size_t on = t.lightTable - 6;
  const std::size_t off = t.lightTable + static_cast<std::size_t>(t.lightCount) * 2;
  if (on + 5 <= d.size())
    out.palette.colorOn = {expandLight(d[on + 2]), expandLight(d[on + 3]), expandLight(d[on + 4])};
  if (off + 5 <= d.size())
    out.palette.colorOff = {expandSix(d[off + 2]), expandSix(d[off + 3]), expandSix(d[off + 4])};
  return out;
}

}  // namespace encore
