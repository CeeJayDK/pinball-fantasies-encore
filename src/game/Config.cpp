#include "game/Config.h"

#include "core/File.h"

namespace pfr {

namespace {
HighScore hs(const char* name, std::string_view score) {
  return {Bcd::of(score), {static_cast<u8>(name[0]), static_cast<u8>(name[1]), static_cast<u8>(name[2])}};
}
std::filesystem::path hiFile(const std::filesystem::path& dir, int table) {
  return dir / ("TABLE" + std::to_string(table + 1) + ".HI");
}
}  // namespace

Config Config::defaults() {
  Config c;
  c.highScores[0] = {hs("TSP", "50000000"), hs("ICE", "25000000"), hs("ANY", "10000000"), hs("J L", "5000000")};
  c.highScores[1] = {hs("TSP", "100000000"), hs("J L", "50000000"), hs("ICE", "25000000"), hs("ANY", "10000000")};
  c.highScores[2] = {hs("TSP", "50000000"), hs("ANY", "25000000"), hs("J L", "10000000"), hs("ICE", "5000000")};
  c.highScores[3] = {hs("TSP", "100000000"), hs("ICE", "50000000"), hs("ANY", "25000000"), hs("J L", "10000000")};
  return c;
}

Config Config::load(const std::filesystem::path& dir, const std::filesystem::path& fallbackDir) {
  Config c = defaults();
  auto read = [&](const std::string& name) {
    auto b = file::readAll(dir / name);
    if (!b && !fallbackDir.empty())
      if (auto p = file::findCaseInsensitive(fallbackDir, name)) b = file::readAll(*p);
    return b;
  };
  if (auto cfg = read("PINBALL.CFG"); cfg && cfg->size() == 6) {
    const Bytes& b = *cfg;
    c.options.balls = b[0] == 1 ? 5 : 3;
    // 0 high, 1 low (as the DOS setup writes them); 2 is Higher, which the DOS game reads as high.
    c.options.angle = b[1] == 1 ? Angle::Low : b[1] == 2 ? Angle::Higher : Angle::High;
    c.options.scrollSpeed = b[2] == 0 ? ScrollSpeed::Hard : b[2] == 2 ? ScrollSpeed::Soft : ScrollSpeed::Medium;
    c.options.noMusic = b[3] == 1;
    c.options.resolution = b[4] == 1 ? Resolution::High : b[4] == 2 ? Resolution::Full : Resolution::Normal;
    c.options.mono = b[5] == 1;
  }
  for (int t = 0; t < 4; ++t) {
    auto hi = read(hiFile({}, t).string());
    if (!hi || hi->size() != 0x40) continue;
    for (std::size_t i = 0; i < 4; ++i) {
      const ByteView e = ByteView(*hi).subspan(i * 0x10, 0x10);
      c.highScores[static_cast<std::size_t>(t)][i].score = Bcd::fromBytes(e);
      for (std::size_t k = 0; k < 3; ++k) c.highScores[static_cast<std::size_t>(t)][i].name[k] = e[12 + k];
    }
  }
  return c;
}

void Config::saveOptions(const std::filesystem::path& dir, const Options& o) {
  const u8 raw[6] = {static_cast<u8>(o.balls == 5), static_cast<u8>(o.angle == Angle::Low ? 1 : o.angle == Angle::Higher ? 2 : 0), static_cast<u8>(o.scrollSpeed),
                     static_cast<u8>(o.noMusic), static_cast<u8>(o.resolution), static_cast<u8>(o.mono)};
  file::writeAll(dir / "PINBALL.CFG", raw);
}

void Config::saveHighScores(const std::filesystem::path& dir, int table, const HighScores& s) {
  Bytes raw;
  for (const HighScore& h : s) {
    raw.insert(raw.end(), h.score.digits.begin(), h.score.digits.end());
    raw.insert(raw.end(), h.name.begin(), h.name.end());
    raw.push_back(0);
  }
  file::writeAll(hiFile(dir, table), raw);
}

}  // namespace pfr
