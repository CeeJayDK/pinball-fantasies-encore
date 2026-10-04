// encore-oracle-physics: our ball physics against the original's, frame by frame.
//
// The referee plays a table: starts a game, pulls the plunger for `pull` frames and lets go.
// At the frame of the launch our physics is given the ball as the original has it; from then
// on each runs by itself and the two balls are compared after every frame, until they part,
// the ball is lost, or something happens that only the table's rules know about.
//
//   encore-oracle-physics <game folder> <table 1-4> [pull frames] [frames] [bounces]
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Machine.h"
#include "Symbols.h"
#include "engine/data/Bumpers.h"
#include "engine/data/TableData.h"
#include "engine/sim/Physics.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::puts("usage: encore-oracle-physics <game folder> <table 1-4> [pull frames] [frames] [bounces]");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const int tableIndex = std::atoi(argv[2]) - 1;
  if (tableIndex < 0 || tableIndex > 3) return 2;
  const oracle::Symbols& sym = oracle::kSymbols[tableIndex];
  const oracle::u16 kSpin = sym.spin, kX = sym.x, kY = sym.y, kXFixed = sym.xFixed, kYFixed = sym.yFixed, kVx = sym.vx, kVy = sym.vy,
                    kLayer = sym.layer, kHidden = sym.hidden;
  const int pull = argc > 3 ? std::atoi(argv[3]) : 100;
  const int frames = argc > 4 ? std::atoi(argv[4]) : 2000;
  try {
    oracle::Machine m(dir, tableIndex, {});
    const oracle::u16 ds = m.seg(sym.data);
    auto w = [&](oracle::u16 off) { return static_cast<oracle::i16>(m.peek16(ds, off)); };
    auto d = [&](oracle::u16 off) { return static_cast<oracle::i32>(m.peek16(ds, off) | (oracle::u32{m.peek16(ds, static_cast<oracle::u16>(off + 2))} << 16)); };

    const int start = 300, down = 600, up = down + pull;
    for (int f = 0; f <= up + 1; ++f) {
      if (f == start) m.key(0x1c);
      if (f == start + 5) m.key(0x9c);
      if (f == down) m.key(0xe0), m.key(0x50);
      if (f == up) m.key(0xe0), m.key(0xd0);
      m.frame();
    }
    // Wait for the launch itself.
    int f = up + 2;
    for (; f < up + 200 && w(kVy) >= 0; ++f) m.frame();

    const encore::TableData table = encore::TableData::load(dir / ("TABLE" + std::to_string(tableIndex + 1) + ".PRG"), tableIndex);
    encore::Physics physics;
    physics.init(table, true);
    const auto bumpers = encore::extractBumpers(table.dataSegment, tableIndex);
    physics.setBumpers(&bumpers);
    encore::Ball ball;
    ball.xFixed = d(kXFixed);
    ball.yFixed = d(kYFixed);
    ball.x = w(kX);
    ball.y = w(kY);
    ball.vx = w(kVx);
    ball.vy = w(kVy);
    ball.spin = w(kSpin);
    ball.upper = m.peek8(ds, kLayer) != 0;
    ball.active = true;
    std::printf("launched at frame %d: (%d,%d) speed (%d,%d) spin %d layer %d\n", f, ball.x, ball.y, ball.vx, ball.vy, ball.spin, ball.upper);

    // With a third argument of "bounces", every bounce of both is printed.
    int frameNow = 0;
    if (argc > 5 && tableIndex == 0) {
      m.cpu.watch[(oracle::u32{m.seg(0x10)} << 16) | 0x8e95] = [&] {
        std::printf("  %4d original bounce at (%d,%d) speed (%d,%d) angle %03x probes %d material %d flipper (%d,%d)\n", frameNow, w(kX), w(kY),
                    w(kVx), w(kVy), m.peek16(ds, 0x6890), m.peek8(ds, 0x6892), m.peek8(ds, 0x2edc), w(0x6894), w(0x6896));
      };
      physics.onBounce = [&](const encore::Ball& b, const encore::Contact& c) {
        std::printf("  %4d ours     bounce at (%d,%d) speed (%d,%d) angle %03x probes %d material %d\n", frameNow, b.x, b.y, b.vx, b.vy, c.angle,
                    c.count, c.materialClass);
      };
    }
    encore::Physics::Controls controls;
    int same = 0;
    // ENCORE_FLIP=<seed>: both are given the same flipper keys, pressed and let go at random.
    unsigned rng = 0;
    const bool flip = std::getenv("ENCORE_FLIP") != nullptr;
    if (flip) rng = static_cast<unsigned>(std::atoi(std::getenv("ENCORE_FLIP"))) * 2654435761u + 1;
    auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
    for (int i = 0; i < frames; ++i, ++same) {
      frameNow = i;
      if (flip && i > 60) {
        if (random() % 23 == 0) { controls.leftFlipper = !controls.leftFlipper; m.key(controls.leftFlipper ? 0x2a : 0xaa); }
        if (random() % 23 == 0) { controls.rightFlipper = !controls.rightFlipper; m.key(controls.rightFlipper ? 0x36 : 0xb6); }
      }
      m.frame();
      physics.clearContact();
      physics.frameStart(ball, controls);
      physics.frameEnd(ball, controls);
      const bool hidden = m.peek8(ds, kHidden) != 0;
      const bool equal = ball.x == w(kX) && ball.y == w(kY) && ball.vx == w(kVx) && ball.vy == w(kVy) && ball.spin == w(kSpin) &&
                         ball.upper == (m.peek8(ds, kLayer) != 0);
      if (hidden) {  // drained, or taken by one of the table's holes: the rules' business from here
        std::printf("the same for %d frames, until the original took the ball at (%d,%d)\n", same, ball.x, ball.y);
        return 0;
      }
      if (!equal) {
        std::printf("%s after %d frames the same\n  original: (%d,%d) speed (%d,%d) spin %d layer %d%s\n  ours:     (%d,%d) speed (%d,%d) spin %d layer %d\n",
                    "they part", same, w(kX), w(kY), w(kVx), w(kVy), w(kSpin),
                    m.peek8(ds, kLayer) != 0, hidden ? " (hidden)" : "", ball.x, ball.y, ball.vx, ball.vy, ball.spin, ball.upper);
        return 1;
      }
    }
    std::printf("the same for %d frames\n", same);
  } catch (const std::exception& e) {
    std::printf("stopped: %s\n", e.what());
    return 2;
  }
  return 0;
}
