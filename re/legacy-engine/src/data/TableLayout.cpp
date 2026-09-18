#include "data/TableLayout.h"

#include <cmath>
#include <cstring>

#include "core/Log.h"

namespace pfr {
namespace {

/// The shared engine constants sit at this fixed distance from the sine table in every
/// table binary, which was checked against all four.
constexpr std::size_t kLimitsFromSine = 0x22a2;

std::size_t findBytes(ByteView haystack, const Bytes& needle, std::size_t stride = 1) {
  if (needle.empty() || haystack.size() < needle.size()) return 0;
  for (std::size_t i = 0; i + needle.size() <= haystack.size(); i += stride)
    if (std::memcmp(haystack.data() + i, needle.data(), needle.size()) == 0) return i;
  return 0;
}

/// The sine table's contents are fixed by mathematics, so matching its first entries
/// identifies it beyond doubt.
std::size_t findSineTable(ByteView data) {
  Bytes probe;
  for (int i = 0; i < 64; ++i) {
    const int v = static_cast<int>(std::lround(std::sin(i * 2.0 * M_PI / 2048.0) * 16384.0));
    probe.push_back(static_cast<u8>((v >> 8) & 0xff));
    probe.push_back(static_cast<u8>(v & 0xff));
  }
  return findBytes(data, probe);
}

/// Eight consecutive 16-byte records, each five signed words followed by six zero bytes,
/// with the signs and ranges the collision code requires. This is unique in every table.
std::size_t findMaterials(ByteView d) {
  if (d.size() < 128) return 0;
  for (std::size_t o = 0; o + 128 <= d.size(); ++o) {
    bool ok = true;
    for (int r = 0; r < 8 && ok; ++r) {
      const std::size_t q = o + static_cast<std::size_t>(r) * 16;
      for (int t = 10; t < 16; ++t)
        if (d[q + t] != 0) { ok = false; break; }
      if (!ok) break;
      const i16 w0 = static_cast<i16>(rd16le(d, q));
      const i16 w1 = static_cast<i16>(rd16le(d, q + 2));
      const i16 w2 = static_cast<i16>(rd16le(d, q + 4));
      const i16 w3 = static_cast<i16>(rd16le(d, q + 6));
      const i16 w4 = static_cast<i16>(rd16le(d, q + 8));
      ok = w0 > 0 && w1 > 0 && w2 > 0 && w3 < 0 && w3 > -3000 && w4 >= 5 && w4 <= 60;
    }
    if (ok) return o;
  }
  return 0;
}

/// The table-angle option walks both gravity tables with
/// `mov bx,TABLE / ... / mov cx,4 / sub word [bx+2],3 / add bx,4 / loop`.
/// The two immediates loaded into bx are the addresses of the two tables.
bool findGravity(ByteView code, std::size_t& lowRes, std::size_t& highRes) {
  const Bytes pattern = {0x83, 0x6F, 0x02, 0x03, 0x83, 0xC3, 0x04, 0xE2, 0xF7};
  const std::size_t at = findBytes(code, pattern);
  if (at == 0) return false;
  const std::size_t from = at > 40 ? at - 40 : 0;
  std::vector<u16> immediates;
  for (std::size_t i = from; i + 3 <= at; ++i)
    if (code[i] == 0xBB) immediates.push_back(static_cast<u16>(rd16le(code, i + 1)));
  if (immediates.size() < 2) return false;
  lowRes = immediates[immediates.size() - 2];
  highRes = immediates.back();
  return true;
}

/// The flipper set-up writes each record's frame block:
/// `mov bx,RECORDS / mov word [bx+0x3a],SEG / mov word [bx+0x76],SEG / mov word [bx+0xb2],SEG`.
bool findFlippers(ByteView code, std::size_t& records, std::array<u16, 3>& segments) {
  std::size_t at = 0;
  for (std::size_t i = 0; i + 8 < code.size(); ++i) {
    if (code[i] == 0xBB && code[i + 3] == 0xC7 && code[i + 4] == 0x47 && code[i + 5] == 0x3A) {
      at = i;
      break;
    }
  }
  if (at == 0) return false;
  records = rd16le(code, at + 1);
  std::size_t j = at + 3;
  for (int k = 0; k < 3 && j + 6 <= code.size(); ++k) {
    std::size_t displacement = 0, value = 0;
    if (code[j] == 0xC7 && code[j + 1] == 0x47) {           // 8-bit displacement
      displacement = code[j + 2];
      value = rd16le(code, j + 3);
      j += 5;
    } else if (code[j] == 0xC7 && code[j + 1] == 0x87) {    // 16-bit displacement
      displacement = rd16le(code, j + 2);
      value = rd16le(code, j + 4);
      j += 6;
    } else {
      break;
    }
    const std::size_t index = (displacement - 0x3a) / 0x3c;
    if (index < segments.size()) segments[index] = static_cast<u16>(value);
  }
  return true;
}

/// The two layer-transition lists sit back to back: the one used while the ball is on the
/// ramp layer, then the one used while it is on the playfield, each ended by a word of -1. Requiring both
/// lists together, with sensible table coordinates, picks them out uniquely in every binary.
bool findLayerZones(ByteView d, std::size_t& whenUpper, std::size_t& whenLower) {
  auto readList = [&](std::size_t o, int& count, std::size_t& end) {
    count = 0;
    while (o + 8 <= d.size()) {
      const i16 x1 = static_cast<i16>(rd16le(d, o));
      if (x1 == -1) { end = o + 2; return true; }
      const i16 y1 = static_cast<i16>(rd16le(d, o + 2));
      const i16 x2 = static_cast<i16>(rd16le(d, o + 4));
      const i16 y2 = static_cast<i16>(rd16le(d, o + 6));
      if (!(x1 >= 0 && x1 < x2 && x2 <= 320 && y1 >= 0 && y1 < y2 && y2 <= 576)) return false;
      ++count;
      if (count > 24) return false;
      o += 8;
    }
    return false;
  };
  for (std::size_t o = 0; o + 16 < d.size(); ++o) {
    int first = 0, second = 0;
    std::size_t end = 0, unused = 0;
    if (!readList(o, first, end) || first < 3) continue;
    if (!readList(end, second, unused) || second < 2) continue;
    whenUpper = o;
    whenLower = end;
    return true;
  }
  return false;
}

}  // namespace

std::vector<Rect> TableLayout::readZoneList(ByteView d, std::size_t offset) {
  std::vector<Rect> out;
  for (std::size_t o = offset; o + 8 <= d.size(); o += 8) {
    const i16 x1 = static_cast<i16>(rd16le(d, o));
    if (x1 == -1) break;
    const i16 y1 = static_cast<i16>(rd16le(d, o + 2));
    const i16 x2 = static_cast<i16>(rd16le(d, o + 4));
    const i16 y2 = static_cast<i16>(rd16le(d, o + 6));
    if (!(x1 >= 0 && x1 < x2 && y1 >= 0 && y1 < y2)) break;
    out.push_back({x1, y1, x2 - x1, y2 - y1});
  }
  return out;
}

TableLayout TableLayout::resolve(const TableData& table) {
  TableLayout l;
  l.sineTable = findSineTable(table.dataSegment);
  if (l.sineTable != 0) {
    const std::size_t limits = l.sineTable + kLimitsFromSine;
    if (limits + 14 <= table.dataSegment.size()) {
      // Sanity check: the first two words are the negative and positive velocity clamps.
      const i16 lo = static_cast<i16>(rd16le(table.dataSegment, limits));
      const i16 hi = static_cast<i16>(rd16le(table.dataSegment, limits + 2));
      if (lo < 0 && hi > 0 && lo == -hi) l.limits = limits;
    }
  }
  l.materials = findMaterials(table.dataSegment);
  findGravity(table.code, l.gravityLowRes, l.gravityHighRes);
  findFlippers(table.code, l.flipperRecords, l.flipperSegments);
  findLayerZones(table.dataSegment, l.layerZonesWhenUpper, l.layerZonesWhenLower);

  if (!l.complete())
    log::warn(table.name + ": could not locate every engine structure in the binary");
  return l;
}

}  // namespace pfr
