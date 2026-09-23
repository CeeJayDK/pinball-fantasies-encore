#include "intro/IntroAssets.h"

#include <algorithm>
#include <utility>

#include "assets/Exe.h"
#include "data/IffImage.h"

namespace pfr {

namespace {

IntroImage parse(const Exe& exe, u16 seg) {
  const auto img = decodeIff(exe.segment(seg));
  if (!img) throw DataError("unreadable picture in INTRO.PRG");
  IntroImage r;
  r.data = Grid8(img->width, img->height);
  for (int y = 0; y < img->height; ++y)
    for (int x = 0; x < img->width; ++x) r.data(x, y) = img->at(x, y);
  r.cmap = img->palette;
  return r;
}

/// Crops to (w, h), padding with zeros where the picture is smaller.
IntroImage crop(IntroImage img, int w, int h) {
  Grid8 g(w, h);
  for (int y = 0; y < h && y < img.data.height(); ++y)
    for (int x = 0; x < w && x < img.data.width(); ++x) g(x, y) = img.data(x, y);
  img.data = std::move(g);
  return img;
}

/// Stacks the first `upperRows` rows of `upper` over the rest of the 240 from `lower`.
IntroImage stack(const IntroImage& upper, const IntroImage& lower, int upperRows) {
  IntroImage r;
  r.cmap = upper.cmap;
  r.data = Grid8(320, 240);
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 320; ++x) {
      const IntroImage& src = y < upperRows ? upper : lower;
      const int sy = y < upperRows ? y : y - upperRows;
      if (sy < src.data.height() && x < src.data.width()) r.data(x, y) = src.data(x, sy);
    }
  return r;
}

std::vector<std::vector<u8>> leftText(const Exe& exe, u16 off) {
  std::vector<std::vector<u8>> r;
  for (u16 i = 0; i < 10; ++i) {
    const ByteView b = exe.codeBytes(static_cast<u16>(off + i * 12), 12);
    r.emplace_back(b.begin(), b.end());
  }
  return r;
}

}  // namespace

IntroAssets IntroAssets::load(ByteView prg) {
  Exe exe = Exe::load(prg, 0x80);
  IntroAssets a;

  IntroImage logo0 = stack(parse(exe, 0x3b41), parse(exe, 0x4285), 139);
  logo0.cmap.resize(std::max<std::size_t>(logo0.cmap.size(), 0x40));
  for (std::size_t i = 0; i < 0x20; ++i)
    logo0.cmap[i + 0x20] = {static_cast<u8>(logo0.cmap[i].r / 2), static_cast<u8>(logo0.cmap[i].g / 2),
                            static_cast<u8>(logo0.cmap[i].b / 2)};
  const IntroImage logo1 = stack(parse(exe, 0x4d6a), parse(exe, 0x4653), 125);
  const IntroImage logo2 = stack(parse(exe, 0x11c2), parse(exe, 0x17b2), 110);
  const IntroImage presents = crop(parse(exe, 0x10a6), 320, 240);
  IntroImage pflogo = parse(exe, 0xa05);
  {
    // The title picture sits between blank bands, 150 rows above and 152 below.
    Grid8 g(640, 150 + pflogo.data.height() + 152);
    g.paste(0, 150, pflogo.data);
    pflogo.data = std::move(g);
  }
  a.slides = {
      {logo0, 0, 20, 20, 0x12e, false},  {logo1, 0, 10, 20, 0x26c, true}, {logo2, 1, 20, 20, 0x37b, false},
      {presents, 0, 8, 20, 1, false},    {pflogo, 1, 20, 20, 0x5dc, false},
  };

  a.left = crop(parse(exe, 0x1c71), 130, 240);
  static constexpr u16 kTableSegs[4] = {0x2003, 0x2465, 0x2901, 0x2d94};
  for (std::size_t i = 0; i < 4; ++i) a.tables[i] = crop(parse(exe, kTableSegs[i]), 440, 95);
  a.fontHq = parse(exe, 0x677);
  a.fontLq = parse(exe, 0x870);
  a.fontLq = crop(a.fontLq, a.fontLq.data.width(), 28);
  a.hiscoresLq = crop(parse(exe, 0x3301), 400, 40);
  a.hiscoresHq = crop(parse(exe, 0x3499), 400, 40);

  for (u16 i = 0; i < 10; ++i) {
    u16 ptr = exe.dataWord(static_cast<u16>(0x4c03 + i * 2));
    TextPage page;
    if (ptr == 0xffff) {
      page.hiScores = true;
      page.tables34 = (i & 1) != 0;
    } else {
      for (int l = 0; l < 12; ++l) {
        std::vector<u8> line;
        for (int c = 0; c < 24; ++c) {
          const u8 byte = exe.dataByte(ptr++);
          if (byte == 0) break;
          line.push_back(byte);
        }
        page.lines.push_back(std::move(line));
      }
    }
    a.textPages.push_back(std::move(page));
  }
  a.leftTextMenu = leftText(exe, 0x288e);
  a.leftTextOptions = leftText(exe, 0x2906);

  std::vector<int> warp;
  for (u16 pos = 0x597a;;) {
    const u8 byte = exe.dataByte(pos++);
    if (byte == 0xff) {
      ++a.warpFrames;
      if (exe.dataByte(pos) == 0xff) break;
    } else {
      if (warp.size() <= byte) warp.resize(byte + 1u, -1);
      warp[byte] = a.warpFrames;
    }
  }
  for (int w : warp) a.warpTable.push_back(static_cast<u8>(w < 0 ? 0 : w));
  if (a.warpTable.size() < 95) a.warpTable.resize(95, a.warpFrames);
  return a;
}

}  // namespace pfr
