// Plays the real table module for a while when the game files are available (skipped otherwise).
#include <cmath>
#include <filesystem>

#include "Test.h"
#include "audio/ModPlayer.h"
#include "core/File.h"

using namespace pfr;

TEST(mod_real_module_plays_through) {
  const std::filesystem::path dir = std::filesystem::path(PFR_SOURCE_DIR) / "..";
  auto path = file::findCaseInsensitive(dir, "TABLE1.MOD");
  if (!path) { std::printf("  (skipped: TABLE1.MOD not found)\n"); return; }
  ModFile mod = ModFile::load(*path);
  CHECK_EQ(mod.title, std::string("pinball2-table1"));
  CHECK_EQ(mod.songLength, 64);
  ModPlayer p(48000);
  p.setModule(&mod);
  p.play();
  std::vector<float> out(4800 * 2);
  double energy = 0;
  bool finite = true;
  float peak = 0;
  for (int s = 0; s < 120; ++s) {  // 12 seconds
    p.render(out.data(), 4800);
    for (float v : out) {
      if (!std::isfinite(v)) finite = false;
      energy += std::fabs(v);
      peak = std::max(peak, std::fabs(v));
    }
  }
  CHECK(finite);
  CHECK(energy / (120.0 * 4800 * 2) > 0.01);
  CHECK(peak <= 1.0f);
  CHECK(p.position() > 0 || p.row() > 0);
}
