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
#include "core/Inflate.h"

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
  if (!inflateZlib(idat, raw)) return std::nullopt;
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
