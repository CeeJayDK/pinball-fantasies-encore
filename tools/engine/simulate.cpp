// encore-simulate: runs a table headlessly and prints the ball's path.
// Used to check the simulation without opening a window.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>

#include "core/Error.h"
#include "core/Png.h"
#include "engine/data/PhysmapPatches.h"
#include "engine/game/Play.h"

namespace {
/// Names for Party Land's handlers, so the trace reads as table features rather than addresses.
const char* partyLandName(bool rolled, unsigned handler) {
  if (!rolled) switch (handler) {
    case 0x134c: return "arcade button";
    case 0x13ae: return "duck 1";
    case 0x142e: return "duck 2";
    case 0x14ae: return "duck 3";
    default: return nullptr;
  }
  switch (handler) {
    case 0x16b6: return "top left orbit";
    case 0x18af: return "top right orbit";
    case 0x1a0e: return "secret passage";
    case 0x1b29: return "tunnel";
    case 0x1cda: return "arcade";
    case 0x1f92: return "snack ramp";
    case 0x225d: return "right orbit entry";
    case 0x2264: return "enter";
    case 0x2299: return "demon";
    case 0x24bc: case 0x24d7: return "inner lane";
    case 0x24f2: case 0x2577: return "outer lane";
    default: return nullptr;
  }
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::puts("usage: encore-simulate <TABLEn.PRG> [frames] [everyNframes] [out.png] [lit]");
    return 2;
  }
  const std::filesystem::path prg = argv[1];
  const int frames = argc > 2 ? std::atoi(argv[2]) : 700;
  const int every = argc > 3 ? std::atoi(argv[3]) : 35;
  const char* png = argc > 4 ? argv[4] : nullptr;
  const bool allLit = argc > 5 && std::string(argv[5]) == "lit";
  try {
    const std::string stem = prg.stem().string();
    int index = 0;
    if (!stem.empty() && stem.back() >= '1' && stem.back() <= '4') index = stem.back() - '1';
    encore::TableData table = encore::TableData::load(prg, index);
    encore::Play play;
    play.init(table, true);
    if (allLit)
      for (std::size_t i = 0; i < play.lightCount(); ++i) play.setLight(i, true);
    encore::PlayInput in;

    in.plunger = true;
    for (int f = 0; f < 40; ++f) play.update(in, 1.0 / 70.0);
    in.plunger = false;
    std::printf("%s: plunger released\n", table.name.c_str());

    // ENCORE_MAP=<file.png>: the top 160 rows of both layers' walls with the ball's path.
    if (const char* a = std::getenv("ENCORE_ASCII")) {
      int x0, x1, y0, y1;
      std::sscanf(a, "%d,%d,%d,%d", &x0, &x1, &y0, &y1);
      for (int y = y0; y < y1; ++y) {
        std::printf("%3d ", y);
        for (int x = x0; x < x1; ++x) {
          const bool r = table.ramps.get(x, y), w = table.walls.get(x, y);
          std::putchar(r && w ? '#' : r ? 'R' : w ? '.' : ' ');
        }
        std::putchar('\n');
      }
    }
    const char* mapPng = std::getenv("ENCORE_MAP");
    if (std::getenv("ENCORE_PATCHES"))
      for (const auto& p : encore::extractPhysmapPatches(table.dataSegment, index))
        std::printf("patch %s x %d..%d y %d..%d\n", p.overhead ? "overhead" : "ground", p.byteX * 8,
                    (p.byteX + p.byteWidth) * 8, p.y, p.y + p.height);
    std::vector<encore::u8> map(320 * 160, 0);
    if (mapPng)
      for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 320; ++x)
          map[static_cast<std::size_t>(y * 320 + x)] =
              static_cast<encore::u8>((table.walls.get(x, y) ? 1 : 0) | (table.ramps.get(x, y) ? 2 : 0));
    int stuckFor = 0;
    unsigned long long lastScore = 0;
    encore::Point last{play.ball().x, play.ball().y};
    for (int f = 0; f < frames; ++f) {
      play.update(in, 1.0 / 70.0);
      const encore::Ball& b = play.ball();
      if (!b.active) {
        std::printf("frame %5d: drained\n", f);
        break;
      }
      if (mapPng && b.y + 8 >= 0 && b.y + 8 < 160 && b.x + 8 >= 0 && b.x + 8 < 320)
        map[static_cast<std::size_t>((b.y + 8) * 320 + b.x + 8)] = b.upper ? 5 : 4;
      if (b.x == last.x && b.y == last.y) ++stuckFor; else stuckFor = 0;
      last = {b.x, b.y};
      // Hold the flippers just before the capture so their travel is visible.
      if (play.score().value() != lastScore) {
        std::printf("frame %5d: score %s\n", f, play.score().text().c_str());
        lastScore = play.score().value();
      }
      for (const auto& e : play.events()) {
        const char* name = index == 0 ? partyLandName(e.rolled, e.handler) : nullptr;
        std::printf("frame %5d: %s %s\n", f, e.rolled ? "rolled over" : "hit",
                    name ? name : ("handler 0x" + [](unsigned v){ char b[8]; std::snprintf(b, sizeof b, "%04x", v); return std::string(b); }(e.handler)).c_str());
      }
      if (png && f > frames - 12) { in.leftFlipper = true; in.rightFlipper = true; }
      if (png && f == frames - 1) {
        encore::Framebuffer frame;
        play.render(frame);
        encore::Palette palette;
        play.applyPalette(palette);
        std::vector<encore::Rgb> colours(palette.colors().begin(), palette.colors().end());
        encore::writeIndexedPng(png, frame.data(), frame.width(), frame.height(), colours);
        std::printf("wrote %s at frame %d\n", png, f);
      }
      if (f % every == 0 || stuckFor == 70)
        std::printf("frame %5d: pos (%3d,%3d) vel (%6d,%6d) spin %5d layer %d camera %3d%s\n", f, b.x, b.y, b.vx,
                    b.vy, b.spin, b.upper ? 1 : 0, play.cameraRow(), stuckFor >= 70 ? "  <- not moving" : "");
    }
    if (mapPng) {
      const std::vector<encore::Rgb> c = {{0, 0, 0}, {90, 90, 255}, {255, 60, 60}, {255, 0, 255}, {0, 255, 0}, {255, 255, 0}};
      // Rows 0..69 at 4x, for reading the top rail.
      std::vector<encore::u8> big(1280 * 280);
      for (int y = 0; y < 280; ++y)
        for (int x = 0; x < 1280; ++x) big[static_cast<std::size_t>(y * 1280 + x)] = map[static_cast<std::size_t>((y / 4) * 320 + x / 4)];
      encore::writeIndexedPng(mapPng, big.data(), 1280, 280, c);
    }
  } catch (const encore::DataError& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
