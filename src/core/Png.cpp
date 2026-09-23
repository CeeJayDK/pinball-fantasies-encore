#include "core/Png.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <optional>
#include <utility>

#include "core/File.h"

namespace pfr {
namespace {

u32 crc32(ByteView data, u32 crc = 0xffffffffu) {
  static u32 table[256];
  static bool init = false;
  if (!init) {
    for (u32 n = 0; n < 256; ++n) {
      u32 c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
      table[n] = c;
    }
    init = true;
  }
  for (u8 b : data) crc = table[(crc ^ b) & 0xff] ^ (crc >> 8);
  return crc;
}

u32 adler32(ByteView data) {
  u32 a = 1, b = 0;
  for (u8 c : data) { a = (a + c) % 65521u; b = (b + a) % 65521u; }
  return (b << 16) | a;
}

void put32(Bytes& out, u32 v) {
  out.push_back(static_cast<u8>(v >> 24)); out.push_back(static_cast<u8>(v >> 16));
  out.push_back(static_cast<u8>(v >> 8)); out.push_back(static_cast<u8>(v));
}

void chunk(Bytes& out, const char* type, ByteView data) {
  put32(out, static_cast<u32>(data.size()));
  Bytes body(type, type + 4);
  body.insert(body.end(), data.begin(), data.end());
  out.insert(out.end(), body.begin(), body.end());
  put32(out, crc32(body) ^ 0xffffffffu);
}

Bytes storedDeflate(ByteView raw) {
  Bytes z = {0x78, 0x01};
  std::size_t pos = 0;
  do {
    const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
    const bool last = pos + n >= raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(static_cast<u8>(n)); z.push_back(static_cast<u8>(n >> 8));
    z.push_back(static_cast<u8>(~n)); z.push_back(static_cast<u8>((~n) >> 8));
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  } while (pos < raw.size());
  put32(z, adler32(raw));
  return z;
}

}  // namespace

bool writeIndexedPng(const std::filesystem::path& path, const u8* pixels, int width, int height, const std::vector<Rgb>& palette) {
  Bytes png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  Bytes ihdr;
  put32(ihdr, static_cast<u32>(width)); put32(ihdr, static_cast<u32>(height));
  ihdr.insert(ihdr.end(), {8, 3, 0, 0, 0});
  chunk(png, "IHDR", ihdr);
  Bytes plte;
  for (std::size_t i = 0; i < 256; ++i) {
    const Rgb c = i < palette.size() ? palette[i] : Rgb{};
    plte.insert(plte.end(), {c.r, c.g, c.b});
  }
  chunk(png, "PLTE", plte);
  Bytes raw;
  raw.reserve(static_cast<std::size_t>(width + 1) * height);
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    raw.insert(raw.end(), pixels + static_cast<std::size_t>(y) * width, pixels + static_cast<std::size_t>(y + 1) * width);
  }
  chunk(png, "IDAT", storedDeflate(raw));
  chunk(png, "IEND", {});
  return file::writeAll(path, png);
}

bool writeRgbPng(const std::filesystem::path& path, const u8* rgb, int width, int height, bool flipVertically) {
  Bytes png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  Bytes ihdr;
  put32(ihdr, static_cast<u32>(width)); put32(ihdr, static_cast<u32>(height));
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
  chunk(png, "IHDR", ihdr);
  Bytes raw;
  raw.reserve(static_cast<std::size_t>(width * 3 + 1) * height);
  for (int y = 0; y < height; ++y) {
    const int sy = flipVertically ? height - 1 - y : y;
    raw.push_back(0);
    raw.insert(raw.end(), rgb + static_cast<std::size_t>(sy) * width * 3, rgb + static_cast<std::size_t>(sy + 1) * width * 3);
  }
  chunk(png, "IDAT", storedDeflate(raw));
  chunk(png, "IEND", {});
  return file::writeAll(path, png);
}

// ---- reading ----------------------------------------------------------------------------

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

/// Undoes the deflate stream inside a zlib wrapper (RFC 1950 and 1951).
bool inflate(ByteView z, Bytes& out) {
  if (z.size() < 2 || (z[0] & 0x0f) != 8) return false;
  BitReader in{ByteView(z.data() + 2, z.size() - 2)};
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

u8 paeth(int a, int b, int c) {
  const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  return static_cast<u8>(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

/// Undoes the filter each row of a PNG is stored with, in place.
bool unfilter(Bytes& raw, int width, int height, int channels) {
  const std::size_t stride = static_cast<std::size_t>(width) * channels;
  if (raw.size() < (stride + 1) * static_cast<std::size_t>(height)) return false;
  Bytes out(stride * static_cast<std::size_t>(height));
  for (int y = 0; y < height; ++y) {
    const u8 filter = raw[static_cast<std::size_t>(y) * (stride + 1)];
    const u8* src = raw.data() + static_cast<std::size_t>(y) * (stride + 1) + 1;
    u8* dst = out.data() + static_cast<std::size_t>(y) * stride;
    const u8* up = y ? dst - stride : nullptr;
    for (std::size_t x = 0; x < stride; ++x) {
      const int a = x >= static_cast<std::size_t>(channels) ? dst[x - static_cast<std::size_t>(channels)] : 0;
      const int b = up ? up[x] : 0;
      const int c = up && x >= static_cast<std::size_t>(channels) ? up[x - static_cast<std::size_t>(channels)] : 0;
      switch (filter) {
        case 0: dst[x] = src[x]; break;
        case 1: dst[x] = static_cast<u8>(src[x] + a); break;
        case 2: dst[x] = static_cast<u8>(src[x] + b); break;
        case 3: dst[x] = static_cast<u8>(src[x] + (a + b) / 2); break;
        case 4: dst[x] = static_cast<u8>(src[x] + paeth(a, b, c)); break;
        default: return false;
      }
    }
  }
  raw = std::move(out);
  return true;
}

}  // namespace

std::optional<RgbaPng> readPng(const std::filesystem::path& path) {
  const auto file = file::readAll(path);
  if (!file) return std::nullopt;
  ByteView png(*file);
  static constexpr u8 kSignature[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  if (png.size() < 8 || !std::equal(std::begin(kSignature), std::end(kSignature), png.begin())) return std::nullopt;

  RgbaPng out;
  int depth = 0, colour = 0, channels = 0;
  Bytes idat;
  std::vector<Rgb> palette;
  std::vector<u8> alpha;
  for (std::size_t p = 8; p + 8 <= png.size();) {
    const u32 length = (static_cast<u32>(png[p]) << 24) | (static_cast<u32>(png[p + 1]) << 16) |
                       (static_cast<u32>(png[p + 2]) << 8) | png[p + 3];
    const char* type = reinterpret_cast<const char*>(png.data() + p + 4);
    const std::size_t body = p + 8;
    if (body + length + 4 > png.size()) return std::nullopt;
    if (!std::strncmp(type, "IHDR", 4)) {
      if (length < 13) return std::nullopt;
      out.width = static_cast<int>((static_cast<u32>(png[body]) << 24) | (static_cast<u32>(png[body + 1]) << 16) |
                                   (static_cast<u32>(png[body + 2]) << 8) | png[body + 3]);
      out.height = static_cast<int>((static_cast<u32>(png[body + 4]) << 24) | (static_cast<u32>(png[body + 5]) << 16) |
                                    (static_cast<u32>(png[body + 6]) << 8) | png[body + 7]);
      depth = png[body + 8];
      colour = png[body + 9];
      // Interlaced pictures are not written by the tools here, and are not read back.
      if (png[body + 12] != 0 || depth != 8) return std::nullopt;
      channels = colour == 0 ? 1 : colour == 2 ? 3 : colour == 3 ? 1 : colour == 4 ? 2 : colour == 6 ? 4 : 0;
      if (!channels || out.width <= 0 || out.height <= 0) return std::nullopt;
    } else if (!std::strncmp(type, "PLTE", 4)) {
      for (u32 i = 0; i + 2 < length; i += 3) palette.push_back({png[body + i], png[body + i + 1], png[body + i + 2]});
    } else if (!std::strncmp(type, "tRNS", 4)) {
      alpha.assign(png.begin() + static_cast<std::ptrdiff_t>(body), png.begin() + static_cast<std::ptrdiff_t>(body + length));
    } else if (!std::strncmp(type, "IDAT", 4)) {
      idat.insert(idat.end(), png.begin() + static_cast<std::ptrdiff_t>(body),
                  png.begin() + static_cast<std::ptrdiff_t>(body + length));
    } else if (!std::strncmp(type, "IEND", 4)) {
      break;
    }
    p = body + length + 4;
  }
  if (!channels || idat.empty()) return std::nullopt;

  Bytes raw;
  raw.reserve((static_cast<std::size_t>(out.width) * channels + 1) * static_cast<std::size_t>(out.height));
  if (!inflate(idat, raw)) return std::nullopt;
  if (!unfilter(raw, out.width, out.height, channels)) return std::nullopt;

  out.pixels.resize(static_cast<std::size_t>(out.width) * out.height * 4);
  for (std::size_t i = 0, n = static_cast<std::size_t>(out.width) * out.height; i < n; ++i) {
    const u8* src = raw.data() + i * static_cast<std::size_t>(channels);
    u8* dst = out.pixels.data() + i * 4;
    switch (colour) {
      case 0: dst[0] = dst[1] = dst[2] = src[0]; dst[3] = 255; break;
      case 2: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255; break;
      case 3: {
        const std::size_t idx = src[0];
        const Rgb c = idx < palette.size() ? palette[idx] : Rgb{};
        dst[0] = c.r; dst[1] = c.g; dst[2] = c.b;
        dst[3] = idx < alpha.size() ? alpha[idx] : 255;
        break;
      }
      case 4: dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; break;
      default: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; break;
    }
  }
  return out;
}

}  // namespace pfr
