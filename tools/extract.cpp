// encore-extract: saves the game's graphics as PNG files, to look at or edit.
//
//   encore-extract <game folder> <output folder>
//
// Everything is read from your own game files; the output is for your use only.
#include <cstdio>
#include <string>
#include <vector>

#include "assets/TableAssets.h"
#include "core/File.h"
#include "core/Png.h"
#include "intro/IntroAssets.h"

namespace fs = std::filesystem;
using namespace pfr;

namespace {

int written = 0;

void save(const fs::path& path, const Grid8& g, const std::vector<Rgb>& palette) {
  std::vector<Rgb> pal = palette;
  pal.resize(256);
  if (!writeIndexedPng(path, g.raw().data(), g.width(), g.height(), pal))
    std::fprintf(stderr, "cannot write %s\n", path.string().c_str());
  else
    ++written;
}

/// A two-colour picture of a bit test on each pixel (black where set, white elsewhere).
void saveMask(const fs::path& path, const Grid8& g, u8 bit) {
  Grid8 m(g.width(), g.height());
  for (int y = 0; y < g.height(); ++y)
    for (int x = 0; x < g.width(); ++x) m(x, y) = (g(x, y) & bit) ? 0 : 1;
  save(path, m, {{0, 0, 0}, {255, 255, 255}});
}

/// The table's palette with every light on (as in play) or off (half brightness).
std::vector<Rgb> tablePalette(const TableAssets& a, bool lit) {
  std::vector<Rgb> pal = a.palette;
  for (const Light& l : a.lights)
    for (std::size_t i = 0; i < l.colors.size(); ++i) {
      const Rgb c = l.colors[i];
      pal[l.baseIndex + i] = lit ? c : Rgb{static_cast<u8>(c.r / 2), static_cast<u8>(c.g / 2), static_cast<u8>(c.b / 2)};
    }
  return pal;
}

void extractTable(const fs::path& game, const fs::path& out, int t) {
  const std::string n = std::to_string(t + 1);
  const auto prg = file::readAll(game / ("TABLE" + n + ".PRG"));
  if (!prg) throw DataError("cannot read TABLE" + n + ".PRG");
  const TableAssets a = TableAssets::load(*prg, t);
  const fs::path dir = out / ("table" + n);
  fs::create_directories(dir / "flippers");
  const auto lit = tablePalette(a, true), unlit = tablePalette(a, false);
  save(dir / "playfield_lights_on.png", a.mainBoard, lit);
  save(dir / "playfield_lights_off.png", a.mainBoard, unlit);
  save(dir / "playfield_original_palette.png", a.mainBoard, a.palette);
  save(dir / "ball.png", a.ball, lit);
  save(dir / "plunger.png", a.spring, lit);
  for (std::size_t f = 0; f < a.flippers.size(); ++f)
    for (std::size_t q = 0; q < a.flippers[f].gfx.size(); ++q) {
      char name[64];
      std::snprintf(name, sizeof name, "flipper%zu_frame%02zu.png", f + 1, q);
      save(dir / "flippers" / name, a.flippers[f].gfx[q], lit);
    }
  // Where the ball is hidden behind the artwork, on the playfield and on the ramps.
  saveMask(dir / "hides_ball_playfield.png", a.occmaps[0], 1);
  saveMask(dir / "hides_ball_ramps.png", a.occmaps[1], 1);
  // What the ball collides with, on each level.
  saveMask(dir / "walls_playfield.png", a.physmaps[0], 2);
  saveMask(dir / "walls_ramps.png", a.physmaps[1], 2);
}

void extractIntro(const fs::path& game, const fs::path& out) {
  const auto prg = file::readAll(game / "INTRO.PRG");
  if (!prg) throw DataError("cannot read INTRO.PRG");
  const IntroAssets a = IntroAssets::load(*prg);
  const fs::path dir = out / "intro";
  fs::create_directories(dir);
  for (std::size_t i = 0; i < a.slides.size(); ++i)
    save(dir / ("slide" + std::to_string(i + 1) + ".png"), a.slides[i].image.data, a.slides[i].image.cmap);
  save(dir / "menu_left_panel.png", a.left.data, a.left.cmap);
  for (std::size_t i = 0; i < 4; ++i)
    save(dir / ("menu_table" + std::to_string(i + 1) + ".png"), a.tables[i].data, a.tables[i].cmap);
  save(dir / "menu_font.png", a.fontHq.data, a.fontHq.cmap);
  save(dir / "menu_font_dim.png", a.fontLq.data, a.fontLq.cmap);
  save(dir / "menu_highscores_title.png", a.hiscoresHq.data, a.hiscoresHq.cmap);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::puts("usage: encore-extract <game folder> <output folder>");
    return 2;
  }
  try {
    const fs::path game = argv[1], out = argv[2];
    for (int t = 0; t < 4; ++t) extractTable(game, out, t);
    extractIntro(game, out);
    std::printf("wrote %d images to %s\n", written, out.string().c_str());
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
