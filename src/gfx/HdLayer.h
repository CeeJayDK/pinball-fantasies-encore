#pragma once
// Replacement pictures in high resolution. A screen that draws one of the original pictures
// also records, for every pixel it drew from it, which picture it was and where in it. The
// renderer then draws the replacement over exactly those pixels at the window's resolution,
// so whatever the screen does with the picture (warps, slides, text drawn over it) still
// applies, and everything else stays as the original drew it.
#include <array>
#include <vector>

#include "core/Types.h"

namespace pfr {

enum class HdPicture : u16 {
  None,
  Slide1, Slide2, Slide3, Slide4, Slide5,  // the opening slideshow
  Left,                                    // the menu's side panel
  Table1, Table2, Table3, Table4,          // the menu's table banners
  HiScores,                                // the high-score pages' heading
  // Each table's playfield with every lamp lit, and with every lamp off.
  Playfield1On, Playfield2On, Playfield3On, Playfield4On,
  Playfield1Off, Playfield2Off, Playfield3Off, Playfield4Off,
  Count,
};

/// The file name (without .png) a replacement is read from.
inline const char* hdPictureName(HdPicture p) {
  static constexpr const char* kNames[] = {"", "slide1", "slide2", "slide3", "slide4", "slide5",
                                           "left", "table1", "table2", "table3", "table4", "hiscores",
                                           "playfield1_on", "playfield2_on", "playfield3_on", "playfield4_on",
                                           "playfield1_off", "playfield2_off", "playfield3_off", "playfield4_off"};
  return kNames[static_cast<std::size_t>(p)];
}

/// One screen pixel: the position of its top-left corner in the original picture, in eighths
/// of a pixel, and the picture. The picture's bits 8 and 9 say that a screen pixel covers half
/// a picture pixel across or down (the picture is drawn doubled in that direction).
struct HdPixel {
  u16 x8 = 0, y8 = 0, picture = 0, unused = 0;
};

struct HdFrame {
  static constexpr u16 kHalfX = 0x100, kHalfY = 0x200;
  static constexpr std::size_t kCount = static_cast<std::size_t>(HdPicture::Count);

  int width = 0, height = 0;
  std::vector<HdPixel> map;                       ///< width x height, top row first
  std::array<std::array<u16, 2>, kCount> size{};  ///< each picture's original size
  u32 used = 0;                                   ///< bit per picture drawn this frame
  std::array<float, kCount> fade{};               ///< per picture: 1 = as drawn, 0 = all fadeColor
  Rgb fadeColor{};

  void reset(int w, int h) {
    width = w;
    height = h;
    map.assign(static_cast<std::size_t>(w) * h, HdPixel{});
    used = 0;
    fade.fill(1.0f);
    fadeColor = {};
  }
};

}  // namespace pfr
