// encore-extract: each table's playfield as the original draws it, written out as pictures:
// with every lamp lit and with every lamp out (what tools/hd_unlit.py works from).
//   encore-extract <the game's folder> <out folder>     -> <out>/table<n>/playfield_lights_on.png, ..._off.png
#include <cstdio>
#include <string>
#include <vector>

#include "core/File.h"
#include "core/Png.h"
#include "engine/game/TableGame.h"

using namespace encore;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::puts("usage: encore-extract <the game's folder> <out folder>");
    return 2;
  }
  const std::filesystem::path dir = argv[1], out = argv[2];
  for (int table = 0; table < 4; ++table) {
    const std::string n = std::to_string(table + 1);
    const auto prg = file::findCaseInsensitive(dir, "TABLE" + n + ".PRG"), mod = file::findCaseInsensitive(dir, "TABLE" + n + ".MOD");
    if (!prg || !mod) {
      std::printf("table %s: its files are not there\n", n.c_str());
      return 1;
    }
    TableGame::Setup setup;
    setup.options.resolution = Resolution::Full;
    TableGame game(*file::readAll(*prg), *file::readAll(*mod), table, setup);
    std::vector<u8> pixels(320 * static_cast<std::size_t>(game.screenHeight()));
    std::vector<Rgb> colours(256);
    std::filesystem::create_directories(out / ("table" + n));
    for (const int lamps : {1, 2}) {
      game.showLamps(lamps);
      game.draw(pixels.data(), colours.data());
      // (the rows below the playfield are the display's)
      writeIndexedPng(out / ("table" + n) / (lamps == 1 ? "playfield_lights_on.png" : "playfield_lights_off.png"), pixels.data(), 320, 576, colours);
    }
    std::printf("table %s written\n", n.c_str());
  }
  return 0;
}
