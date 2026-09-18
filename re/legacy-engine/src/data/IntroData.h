#pragma once
// Everything the intro and menu need, decoded from INTRO.PRG.
//
// The menu is a 640-wide, 16-colour screen assembled from several pictures stacked into one
// tall buffer. Each picture carries its own 16-colour map, and the original displayed them
// together by changing the palette part-way down the screen, using the scanline callback the
// sound driver provides. We reproduce that with a palette per row.
#include <array>
#include <filesystem>
#include <optional>

#include "data/IffImage.h"
#include "data/MzImage.h"

namespace pfr {

struct IntroData {
  static constexpr int kMenuWidth = 640;

  MzImage image;                      ///< keeps the executable's bytes alive
  std::vector<IffImage> pictures;     ///< every picture in the file, in file order
  std::vector<u16> pictureSegments;   ///< the segment each picture was found at

  /// One band of the assembled menu screen: which picture, and where it starts.
  struct MenuBand {
    int picture = 0;
    int row = 0;
    int height = 0;
  };

  /// One screen of the opening sequence: a window onto the stacked intro buffer, with the
  /// palette that screen is shown under.
  struct IntroScreen {
    int row = 0;
    int height = 0;
    std::array<Rgb, 256> palette{};
  };

  static constexpr int kIntroWidth = 320;
  Bytes introPixels;                   ///< kIntroWidth * introHeight palette indices
  int introHeight = 0;
  std::vector<IntroScreen> introScreens;
  int presentsPicture = -1;            ///< the "Presents" card, shown after the logos
  int titlePicture = -1;               ///< the wide title logo that ends the sequence

  std::vector<MenuBand> menuBands;
  Bytes menuPixels;                   ///< kMenuWidth * menuHeight palette indices
  int menuHeight = 0;
  std::vector<std::array<Rgb, 16>> menuRowPalettes;  ///< one 16-colour map per row

  static IntroData load(const std::filesystem::path& introPrg);

  /// Index of the picture stored at a segment value, or -1.
  int pictureAt(u16 segment) const;
};

}  // namespace pfr
