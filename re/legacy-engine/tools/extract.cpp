// pfr-extract: dumps the pictures and masks of a table executable as PNG files.
// Useful for verifying the loaders and for documentation.
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/Error.h"
#include "data/TableData.h"
#include "core/Png.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::puts("usage: pfr-extract <TABLEn.PRG> <output dir>");
    return 2;
  }
  try {
    const std::filesystem::path prg = argv[1];
    const std::filesystem::path out = argv[2];
    std::filesystem::create_directories(out);
    const std::string stem = prg.stem().string();
    int index = 0;
    if (!stem.empty() && stem.back() >= '1' && stem.back() <= '4') index = stem.back() - '1';
    pfr::TableData t = pfr::TableData::load(prg, index);
    pfr::writeIndexedPng(out / (stem + "_playfield.png"), t.playfield.data(), t.playfield.size() ? pfr::TableData::kWidth : 0,
                         pfr::TableData::kHeight, t.palette);
    auto dumpMask = [&](const pfr::Mask& m, const std::string& name) { pfr::writeMaskPng(out / (stem + "_" + name + ".png"), m); };
    dumpMask(t.walls, "walls");
    dumpMask(t.dynamic, "dynamic");
    dumpMask(t.ramps, "ramps");
    dumpMask(t.occlusionA, "occlusionA");
    dumpMask(t.occlusionB, "occlusionB");
    dumpMask(t.occlusionC, "occlusionC");
    for (std::size_t i = 0; i < t.flipperBlocks.size(); ++i)
    dumpMask(pfr::Mask::fromPacked(t.flipperBlocks[i], 64, static_cast<int>(t.flipperBlocks[i].size() / 8), 8),
             "flipperblock" + std::to_string(i));
    std::printf("%s: %zu flipper blocks, palette %zu colours, %zu colour ranges\n", stem.c_str(), t.flipperBlocks.size(),
                t.palette.size(), t.colorRanges.size());
  } catch (const pfr::DataError& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
