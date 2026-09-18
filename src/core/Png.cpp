#include "core/Png.h"

#include <cstdio>

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
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
  std::fclose(f);
  return ok;
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
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
  std::fclose(f);
  return ok;
}

}  // namespace pfr
