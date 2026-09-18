// pfr-assets: loads every table's assets and prints a summary, as a quick integrity check.
#include <cstdio>

#include "assets/TableAssets.h"
#include "core/File.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::puts("usage: pfr-assets <game folder>");
    return 2;
  }
  int failures = 0;
  for (int t = 0; t < 4; ++t) {
    const auto path = std::filesystem::path(argv[1]) / ("TABLE" + std::to_string(t + 1) + ".PRG");
    try {
      const auto prg = pfr::file::readAll(path);
      if (!prg) throw std::runtime_error("cannot read " + path.string());
      const pfr::TableAssets a = pfr::TableAssets::load(*prg, t);
      std::size_t binds = 0, effects = 0;
      for (const auto& b : a.scriptBinds) binds += b.has_value();
      for (const auto& e : a.effects) effects += e.has_value();
      std::printf("TABLE%d: %zu lights, %zu flippers, %zu bumpers, %zu+%zu roll, %zu hit, %zu uops, %zu msgs, "
                  "%zu anims, %zu binds, %zu effects, %zu outline, %zu ramps\n",
                  t + 1, a.lights.size(), a.flippers.size(), a.bumpers.size(), a.rollTriggers[0].size(),
                  a.rollTriggers[1].size(), a.hitTriggers.size(), a.scripts.size(), a.msgs.size(), a.anims.size(),
                  binds, effects, a.ballOutline.size(), a.ramps.size());
    } catch (const std::exception& e) {
      std::printf("TABLE%d: FAILED: %s\n", t + 1, e.what());
      ++failures;
    }
  }
  return failures;
}
