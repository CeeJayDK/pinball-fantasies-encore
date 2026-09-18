#include "data/IntroData.h"

#include <cstring>

#include "core/Error.h"
#include "core/Log.h"

namespace pfr {
namespace {

/// The intro program's data segment, and the two tables in it that describe the menu:
/// a list of picture segments and the row each one is drawn at.
constexpr u16 kIntroDataSegment = 0x0080;
constexpr std::size_t kMenuRowTable = 0x5890;
constexpr std::size_t kMenuSegmentTable = 0x589e;
constexpr int kMaxMenuBands = 8;

/// The opening sequence packs its three logo screens into one tall buffer and shows each by
/// moving the display window. These positions are the ones the original's code sets up.
struct IntroPlacement {
  u16 segment;
  int row;
};
constexpr IntroPlacement kIntroPictures[] = {
    {0x3b41, 0}, {0x4285, 139}, {0x4d6a, 240}, {0x4653, 365}, {0x11c2, 492}, {0x17b2, 602},
};
constexpr int kIntroScreenRows[] = {0, 247, 492};
constexpr int kIntroScreenHeight = 240;
constexpr u16 kPresentsSegment = 0x10a6;
constexpr u16 kTitleSegment = 0xa05;

}  // namespace

int IntroData::pictureAt(u16 segment) const {
  for (std::size_t i = 0; i < pictureSegments.size(); ++i)
    if (pictureSegments[i] == segment) return static_cast<int>(i);
  return -1;
}

IntroData IntroData::load(const std::filesystem::path& introPrg) {
  IntroData d;
  d.image = MzImage::load(introPrg);
  const ByteView img = d.image.image();

  // Every picture in the file, remembering the segment it starts at so the menu tables
  // can refer to them.
  for (const IffLocation& loc : findIffForms(img)) {
    auto picture = decodeIff(img.subspan(loc.offset, loc.length));
    if (!picture) continue;
    d.pictures.push_back(std::move(*picture));
    d.pictureSegments.push_back(static_cast<u16>(loc.offset / 16));
  }
  if (d.pictures.empty()) throw DataError(introPrg.string() + ": no pictures found");

  const ByteView ds = d.image.far(kIntroDataSegment, 0);
  for (int i = 0; i < kMaxMenuBands; ++i) {
    const u16 segment = static_cast<u16>(rd16le(ds, kMenuSegmentTable + static_cast<std::size_t>(i) * 2));
    const i16 row = static_cast<i16>(rd16le(ds, kMenuRowTable + static_cast<std::size_t>(i) * 2));
    if (segment == 0) break;
    const int index = d.pictureAt(segment);
    if (index < 0) {
      log::warn("menu band " + std::to_string(i) + " refers to an unknown picture");
      continue;
    }
    d.menuBands.push_back({index, row, d.pictures[static_cast<std::size_t>(index)].height});
  }
  if (d.menuBands.empty()) throw DataError(introPrg.string() + ": the menu layout could not be read");

  for (const MenuBand& b : d.menuBands) d.menuHeight = std::max(d.menuHeight, b.row + b.height);
  d.menuPixels.assign(static_cast<std::size_t>(kMenuWidth) * d.menuHeight, 0);
  d.menuRowPalettes.assign(static_cast<std::size_t>(d.menuHeight), {});

  // Later bands draw over earlier ones, and each band owns the palette of the rows it covers.
  for (const MenuBand& b : d.menuBands) {
    const IffImage& p = d.pictures[static_cast<std::size_t>(b.picture)];
    const bool keyed = p.masking == 2;
    for (int y = 0; y < p.height; ++y) {
      const int dy = b.row + y;
      if (dy < 0 || dy >= d.menuHeight) continue;
      for (int x = 0; x < p.width && x < kMenuWidth; ++x) {
        const u8 v = p.at(x, y);
        if (keyed && v == p.transparent) continue;
        d.menuPixels[static_cast<std::size_t>(dy) * kMenuWidth + x] = v;
      }
      auto& row = d.menuRowPalettes[static_cast<std::size_t>(dy)];
      for (std::size_t c = 0; c < 16; ++c) row[c] = c < p.palette.size() ? p.palette[c] : Rgb{};
    }
  }
  // The opening sequence.
  d.presentsPicture = d.pictureAt(kPresentsSegment);
  d.titlePicture = d.pictureAt(kTitleSegment);
  for (const IntroPlacement& p : kIntroPictures) {
    const int index = d.pictureAt(p.segment);
    if (index < 0) continue;
    const IffImage& picture = d.pictures[static_cast<std::size_t>(index)];
    d.introHeight = std::max(d.introHeight, p.row + picture.height);
  }
  if (d.introHeight > 0) {
    d.introPixels.assign(static_cast<std::size_t>(kIntroWidth) * d.introHeight, 0);
    std::vector<int> paletteSource(static_cast<std::size_t>(d.introHeight), -1);
    for (const IntroPlacement& p : kIntroPictures) {
      const int index = d.pictureAt(p.segment);
      if (index < 0) continue;
      const IffImage& picture = d.pictures[static_cast<std::size_t>(index)];
      const bool keyed = picture.masking == 2;
      for (int y = 0; y < picture.height; ++y) {
        const int dy = p.row + y;
        if (dy < 0 || dy >= d.introHeight) continue;
        for (int x = 0; x < picture.width && x < kIntroWidth; ++x) {
          const u8 v = picture.at(x, y);
          if (keyed && v == picture.transparent) continue;
          d.introPixels[static_cast<std::size_t>(dy) * kIntroWidth + x] = v;
        }
        paletteSource[static_cast<std::size_t>(dy)] = index;
      }
    }
    // Each screen takes the palette of whichever picture fills it.
    for (int row : kIntroScreenRows) {
      IntroScreen screen;
      screen.row = row;
      screen.height = std::min(kIntroScreenHeight, d.introHeight - row);
      int source = -1;
      for (int y = row; y < row + screen.height && y < d.introHeight; ++y)
        if (paletteSource[static_cast<std::size_t>(y)] >= 0) { source = paletteSource[static_cast<std::size_t>(y)]; break; }
      if (source >= 0) {
        const auto& palette = d.pictures[static_cast<std::size_t>(source)].palette;
        for (std::size_t c = 0; c < 256; ++c) screen.palette[c] = c < palette.size() ? palette[c] : Rgb{};
      }
      if (screen.height > 0) d.introScreens.push_back(screen);
    }
  }

  log::info("intro: " + std::to_string(d.pictures.size()) + " pictures, " +
            std::to_string(d.introScreens.size()) + " opening screens, menu " + std::to_string(kMenuWidth) +
            "x" + std::to_string(d.menuHeight) + " in " + std::to_string(d.menuBands.size()) + " bands");
  return d;
}

}  // namespace pfr
