// pfr-play: plays a game headlessly on one table with a simple autopilot, printing the
// ball, score and dot matrix, to check the rules and physics without a window.
//
//   pfr-play <game folder> <table 1-4> [frames] [seed] [out.png]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/File.h"
#include "core/Png.h"
#include "table/Table.h"

namespace {

void printDm(const pfr::Table& t) {
  const auto& dm = t.dotMatrix();
  for (int y = 0; y < 16; y += 2) {
    std::string line;
    for (int x = 0; x < 160; ++x) {
      const bool a = dm[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      const bool b = dm[static_cast<std::size_t>(y + 1)][static_cast<std::size_t>(x)];
      line += a && b ? "█" : a ? "▀" : b ? "▄" : " ";
    }
    std::printf("  |%s|\n", line.c_str());
  }
}

std::string score(const pfr::Table& t) {
  std::string s;
  for (pfr::u8 c : t.scoreMain().toAscii()) s += static_cast<char>(c);
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::puts("usage: pfr-play <game folder> <table 1-4> [frames] [seed] [out.png]");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const int table = std::atoi(argv[2]) - 1;
  const int frames = argc > 3 ? std::atoi(argv[3]) : 3000;
  const unsigned seed = argc > 4 ? static_cast<unsigned>(std::atoi(argv[4])) : 1;
  const char* png = argc > 5 ? argv[5] : nullptr;
  const auto prg = pfr::file::readAll(dir / ("TABLE" + std::to_string(table + 1) + ".PRG"));
  const auto mod = pfr::file::readAll(dir / ("TABLE" + std::to_string(table + 1) + ".MOD"));
  if (!prg || !mod) {
    std::puts("cannot read the table files");
    return 1;
  }
  pfr::Config cfg = pfr::Config::defaults();
  pfr::Table t(*prg, *mod, cfg, table, seed);
  std::vector<float> audio(960 * 2);
  std::vector<std::string> events;
  t.trace = &events;

  using pfr::Key;
  int plungeAt = -1;
  bool left = false, right = false;
  std::string lastScore;
  int lastBall = 0;
  int prevY = 0;
  for (int f = 0; f < frames; ++f) {
    if (f == 30) t.handleKey(Key::Enter, true), t.handleKey(Key::Enter, false);
    const auto pos = t.ballPos();
    // Plunge whenever the ball rests at the spring.
    if (!t.inAttract() && pos[0] >= 290 && pos[1] >= 515 && plungeAt < 0) {
      plungeAt = f;
      t.handleKey(Key::ArrowDown, true);
    }
    if (plungeAt >= 0 && f == plungeAt + 40) t.handleKey(Key::ArrowDown, false);
    if (plungeAt >= 0 && f > plungeAt + 200 && pos[1] < 500) plungeAt = -1;
    // Autopilot: flip when the ball is above a flipper.
    // Flip while the ball falls onto a flipper; never hold, so it is not cradled forever.
    const bool falling = pos[1] > prevY;
    prevY = pos[1];
    const bool zone = falling && pos[1] > 485 && pos[1] < 545;
    const bool wantL = zone && pos[0] < 150;
    const bool wantR = zone && pos[0] >= 130 && pos[0] < 290;
    if (wantL != left) t.handleKey(Key::ShiftLeft, left = wantL);
    if (wantR != right) t.handleKey(Key::ShiftRight, right = wantR);

    t.runFrame();
    for (const auto& e : events) std::printf("frame %5d: %s\n", f, e.c_str());
    events.clear();
    t.player().render(audio.data(), 800);  // keep the music sequencer moving (48000/60)

    const std::string s = score(t);
    if (t.currentBall() != lastBall) {
      std::printf("frame %5d: ball %d\n", f, t.currentBall());
      lastBall = t.currentBall();
    }
    if (f % (std::getenv("PFR_EVERY") ? std::atoi(std::getenv("PFR_EVERY")) : 250) == 0 || (s != lastScore && f % 25 == 0)) {
      std::printf("frame %5d: pos (%3d,%3d) %s score %s%s\n", f, pos[0], pos[1], t.ballOverhead() ? "ramp" : "    ",
                  s.c_str(), t.inAttract() ? " [attract]" : "");
      lastScore = s;
    }
    if (std::getenv("PFR_DM") && f % 500 == 499) printDm(t);
  }
  printDm(t);
  if (png) {
    std::vector<pfr::u8> pixels(320 * static_cast<std::size_t>(t.screenHeight()));
    std::vector<pfr::Rgb> pal(256);
    t.render(pixels.data(), pal.data());
    pfr::writeIndexedPng(png, pixels.data(), 320, t.screenHeight(), pal);
  }
  return 0;
}
