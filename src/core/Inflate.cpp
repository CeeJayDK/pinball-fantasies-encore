#include "core/Inflate.h"

#include <array>
#include <iterator>
#include <vector>

namespace pfr {
namespace {

/// A stream of bits, lowest first, as deflate stores them.
struct BitReader {
  ByteView data;
  std::size_t pos = 0;
  u32 bits = 0;
  int count = 0;
  bool bad = false;

  u32 take(int n) {
    while (count < n) {
      if (pos >= data.size()) { bad = true; return 0; }
      bits |= static_cast<u32>(data[pos++]) << count;
      count += 8;
    }
    const u32 v = bits & ((1u << n) - 1);
    bits >>= n;
    count -= n;
    return v;
  }
  void align() { bits = 0; count = 0; }
};

/// A canonical Huffman table: the codes of each length, and what they stand for.
struct Huffman {
  std::array<u16, 16> firstCode{}, firstIndex{}, countOfLength{};
  std::vector<u16> symbols;

  void build(const u8* lengths, std::size_t n) {
    countOfLength.fill(0);
    for (std::size_t i = 0; i < n; ++i) ++countOfLength[lengths[i]];
    countOfLength[0] = 0;
    u16 code = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
      firstCode[len] = code;
      firstIndex[len] = index;
      code = static_cast<u16>((code + countOfLength[len]) << 1);
      index = static_cast<u16>(index + countOfLength[len]);
    }
    symbols.assign(index, 0);
    std::array<u16, 16> next = firstIndex;
    for (std::size_t i = 0; i < n; ++i)
      if (lengths[i]) symbols[next[lengths[i]]++] = static_cast<u16>(i);
  }

  int decode(BitReader& in) const {
    u32 code = 0;
    for (int len = 1; len < 16; ++len) {
      code = (code << 1) | in.take(1);
      if (in.bad) return -1;
      if (countOfLength[len] && code - firstCode[len] < countOfLength[len])
        return symbols[firstIndex[len] + (code - firstCode[len])];
    }
    return -1;
  }
};

}  // namespace

bool inflateRaw(ByteView z, Bytes& out) {
  BitReader in{z};
  static constexpr u16 kLengthBase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static constexpr u8 kLengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static constexpr u16 kDistBase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
                                      513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
  static constexpr u8 kDistExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
                                      8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  for (;;) {
    const u32 last = in.take(1), kind = in.take(2);
    if (in.bad) return false;
    if (kind == 0) {  // stored
      in.align();
      if (in.pos + 4 > in.data.size()) return false;
      const std::size_t n = static_cast<std::size_t>(in.data[in.pos]) | (static_cast<std::size_t>(in.data[in.pos + 1]) << 8);
      in.pos += 4;
      if (in.pos + n > in.data.size()) return false;
      out.insert(out.end(), in.data.begin() + static_cast<std::ptrdiff_t>(in.pos),
                 in.data.begin() + static_cast<std::ptrdiff_t>(in.pos + n));
      in.pos += n;
    } else if (kind == 1 || kind == 2) {
      Huffman lit, dist;
      if (kind == 1) {  // the fixed tables
        std::array<u8, 288> ll{};
        for (int i = 0; i < 288; ++i) ll[static_cast<std::size_t>(i)] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
        lit.build(ll.data(), ll.size());
        std::array<u8, 30> dl{};
        dl.fill(5);
        dist.build(dl.data(), dl.size());
      } else {  // the tables the stream carries, themselves coded
        const u32 nLit = in.take(5) + 257, nDist = in.take(5) + 1, nLen = in.take(4) + 4;
        static constexpr u8 kOrder[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        std::array<u8, 19> lenLengths{};
        for (u32 i = 0; i < nLen; ++i) lenLengths[kOrder[i]] = static_cast<u8>(in.take(3));
        Huffman lengths;
        lengths.build(lenLengths.data(), lenLengths.size());
        std::vector<u8> all(nLit + nDist, 0);
        for (std::size_t i = 0; i < all.size();) {
          const int sym = lengths.decode(in);
          if (sym < 0) return false;
          if (sym < 16) {
            all[i++] = static_cast<u8>(sym);
          } else if (sym == 16) {
            if (i == 0) return false;
            const u32 n = in.take(2) + 3;
            const u8 prev = all[i - 1];
            for (u32 k = 0; k < n && i < all.size(); ++k) all[i++] = prev;
          } else {
            const u32 n = sym == 17 ? in.take(3) + 3 : in.take(7) + 11;
            for (u32 k = 0; k < n && i < all.size(); ++k) all[i++] = 0;
          }
          if (in.bad) return false;
        }
        lit.build(all.data(), nLit);
        dist.build(all.data() + nLit, nDist);
      }
      for (;;) {
        const int sym = lit.decode(in);
        if (sym < 0) return false;
        if (sym == 256) break;
        if (sym < 256) {
          out.push_back(static_cast<u8>(sym));
          continue;
        }
        const std::size_t li = static_cast<std::size_t>(sym) - 257;
        if (li >= std::size(kLengthBase)) return false;
        const std::size_t len = kLengthBase[li] + in.take(kLengthExtra[li]);
        const int dsym = dist.decode(in);
        if (dsym < 0 || static_cast<std::size_t>(dsym) >= std::size(kDistBase)) return false;
        const std::size_t back = kDistBase[dsym] + in.take(kDistExtra[dsym]);
        if (in.bad || back > out.size()) return false;
        const std::size_t from = out.size() - back;
        for (std::size_t k = 0; k < len; ++k) out.push_back(out[from + k]);
      }
    } else {
      return false;
    }
    if (last) return true;
    if (in.bad) return false;
  }
}

bool inflateZlib(ByteView z, Bytes& out) {
  if (z.size() < 2 || (z[0] & 0x0f) != 8) return false;
  return inflateRaw(ByteView(z.data() + 2, z.size() - 2), out);
}

}  // namespace pfr
