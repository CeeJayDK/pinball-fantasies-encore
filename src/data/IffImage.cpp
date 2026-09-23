#include "data/IffImage.h"

#include <algorithm>
#include <cstring>
#include <optional>

#include "core/Error.h"

namespace pfr {
namespace {

bool tagIs(ByteView b, std::size_t off, const char* tag) {
  return off + 4 <= b.size() && std::memcmp(b.data() + off, tag, 4) == 0;
}

/// ByteRun1 (PackBits) decompression into exactly `need` bytes.
Bytes unpackByteRun1(ByteView src, std::size_t need) {
  Bytes out;
  out.reserve(need);
  std::size_t i = 0;
  while (out.size() < need && i < src.size()) {
    const u8 n = src[i++];
    if (n < 128) {
      const std::size_t count = std::min<std::size_t>(n + 1u, src.size() - i);
      out.insert(out.end(), src.begin() + i, src.begin() + i + count);
      i += count;
    } else if (n != 128) {
      if (i >= src.size()) break;
      out.insert(out.end(), 257u - n, src[i++]);
    }
  }
  out.resize(need, 0);
  return out;
}

}  // namespace

std::vector<IffLocation> findIffForms(ByteView blob) {
  std::vector<IffLocation> out;
  std::size_t pos = 0;
  while (pos + 12 <= blob.size()) {
    if (tagIs(blob, pos, "FORM")) {
      const u32 size = rd32be(blob, pos + 4);
      if ((tagIs(blob, pos + 8, "ILBM") || tagIs(blob, pos + 8, "PBM ")) && pos + 8 + size <= blob.size()) {
        out.push_back({pos, size + 8});
        pos += size + 8;
        continue;
      }
    }
    ++pos;
  }
  return out;
}

std::optional<IffImage> decodeIff(ByteView data) {
  if (!tagIs(data, 0, "FORM") || data.size() < 12) return std::nullopt;
  const u32 formSize = rd32be(data, 4);
  const bool chunky = tagIs(data, 8, "PBM ");
  if (!chunky && !tagIs(data, 8, "ILBM")) return std::nullopt;
  const std::size_t end = std::min<std::size_t>(data.size(), 8u + formSize);

  IffImage img;
  img.chunky = chunky;
  u8 compression = 0;
  ByteView body;
  bool haveHeader = false;
  std::size_t p = 12;
  while (p + 8 <= end) {
    const u32 len = rd32be(data, p + 4);
    const std::size_t chunkStart = p + 8;
    const std::size_t chunkEnd = std::min<std::size_t>(end, chunkStart + len);
    const ByteView c = data.subspan(chunkStart, chunkEnd - chunkStart);
    if (tagIs(data, p, "BMHD") && c.size() >= 20) {
      img.width = rd16be(c, 0);
      // Some strips carry a bogus height (TABLE4 strip 3 says 1219); the game only ever
      // uses 144-row strips, and the BODY length settles it below.
      img.height = rd16be(c, 2);
      img.planes = c[8];
      img.masking = c[9];
      compression = c[10];
      img.transparent = static_cast<u8>(rd16be(c, 12));
      haveHeader = true;
    } else if (tagIs(data, p, "CMAP")) {
      for (std::size_t i = 0; i + 2 < c.size(); i += 3) img.palette.push_back({c[i], c[i + 1], c[i + 2]});
    } else if (tagIs(data, p, "CRNG") && c.size() >= 8) {
      img.ranges.push_back({rd16be(c, 2), rd16be(c, 4), c[6], c[7]});
    } else if (tagIs(data, p, "BODY")) {
      body = c;
    }
    p = chunkStart + len + (len & 1u);
  }
  if (!haveHeader || img.width <= 0 || img.height <= 0) return std::nullopt;

  if (chunky) {
    const std::size_t rowBytes = (static_cast<std::size_t>(img.width) + 1u) & ~std::size_t{1};
    std::size_t need = rowBytes * img.height;
    Bytes raw = compression == 1 ? unpackByteRun1(body, need) : Bytes(body.begin(), body.begin() + std::min(need, body.size()));
    if (compression == 1) {
      // Trust the data over a corrupt header: shrink the height to the rows actually present.
      // unpackByteRun1 pads with zeros, so recompute from the compressed stream length instead.
      Bytes probe;
      std::size_t i = 0, produced = 0;
      while (i < body.size()) {
        const u8 n = body[i++];
        if (n < 128) { produced += n + 1u; i += n + 1u; }
        else if (n != 128) { produced += 257u - n; i += 1; }
      }
      if (produced < need && produced % rowBytes == 0) {
        img.height = static_cast<int>(produced / rowBytes);
        need = produced;
      }
    }
    raw.resize(need, 0);
    img.pixels.resize(static_cast<std::size_t>(img.width) * img.height);
    for (int y = 0; y < img.height; ++y)
      std::memcpy(img.pixels.data() + static_cast<std::size_t>(y) * img.width, raw.data() + y * rowBytes, img.width);
  } else {
    const std::size_t rowBytes = ((static_cast<std::size_t>(img.width) + 15u) / 16u) * 2u;
    const int planesStored = img.planes + ((img.masking == 1) ? 1 : 0);
    const std::size_t need = rowBytes * planesStored * img.height;
    Bytes raw = compression == 1 ? unpackByteRun1(body, need) : Bytes(body.begin(), body.begin() + std::min(need, body.size()));
    raw.resize(need, 0);
    img.pixels.assign(static_cast<std::size_t>(img.width) * img.height, 0);
    for (int y = 0; y < img.height; ++y) {
      for (int pl = 0; pl < img.planes; ++pl) {
        const u8* row = raw.data() + (static_cast<std::size_t>(y) * planesStored + pl) * rowBytes;
        for (int x = 0; x < img.width; ++x)
          if (row[x >> 3] & (0x80u >> (x & 7))) img.pixels[static_cast<std::size_t>(y) * img.width + x] |= static_cast<u8>(1u << pl);
      }
    }
  }
  return img;
}

}  // namespace pfr
