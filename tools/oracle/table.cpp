// encore-oracle-table: the engine's memory against the original's, frame by frame.
//
// The referee starts a table and runs its start-up; the engine is given the memory as that
// leaves it. Both are then given the same keys, and after every frame the engine's data
// segment, the variables among its code and its colours are compared with the original's.
//
//   encore-oracle-table <game folder> <table 1-4> <frames> [--keys <frame>:<scancode>,...]
//                       [--flip <seed>]  plays at random from frame 400 on
//                       [--all]   goes on after a difference, taking the original's value for
//                                 it, and says each place that ever differs once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Machine.h"
#include "core/File.h"
#include "engine/table/Engine.h"
#include "engine/table/PartyLand.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::puts("usage: encore-oracle-table <game folder> <table 1-4> <frames> [--keys <frame>:<scancode>,...] [--all]");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const int table = std::atoi(argv[2]) - 1;
  const int frames = std::atoi(argv[3]);
  std::vector<std::pair<int, int>> keys;
  bool all = false;
  int flip = -1;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--all")) all = true;
    else if (!std::strcmp(argv[i], "--flip") && i + 1 < argc) flip = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) {
      for (const char* p = argv[++i]; *p;) {
        char* end = nullptr;
        const long a = std::strtol(p, &end, 10);
        if (*end != ':') break;
        const long b = std::strtol(end + 1, &end, 16);
        keys.push_back({static_cast<int>(a), static_cast<int>(b)});
        p = *end == ',' ? end + 1 : end;
      }
    }
  }
  const auto prg = pfr::file::readAll(dir / ("TABLE" + std::to_string(table + 1) + ".PRG"));
  if (!prg) {
    std::puts("cannot read the table's program");
    return 2;
  }
  int frame = -1;
  try {
    std::unique_ptr<encore::Engine> made = table == 0 ? std::make_unique<encore::PartyLand>(*prg) : std::make_unique<encore::Engine>(*prg, table);
    encore::Engine& e = *made;
    oracle::Machine m(dir, table, {});
    m.setLoop(0x10, e.F(encore::at::mainLoop));
    m.loopsPerFrame = e.loopsPerFrame;
    m.boot();
    const oracle::u16 ds = m.seg(e.dataSegment()), cs = m.seg(0x10);
    for (unsigned a = 0; a < 0x10000; ++a) {
      e.memory()[a] = m.peek8(ds, static_cast<oracle::u16>(a));
      e.codeMemory()[a] = m.peek8(cs, static_cast<oracle::u16>(a));
    }
    e.colours() = m.vga.dac;
    for (int p = 0; p < 2; ++p)
      for (unsigned a = 0; a < 0x4000; ++a) e.displayMemory()[static_cast<std::size_t>(p)][a] = m.vga.planes[static_cast<std::size_t>(p * 2)][a];

    // The data segment ends where the next begins; the code's variables are among its code.
    const unsigned dataEnd = 0x6c00, codeEnd = 0xac00;
    std::set<unsigned> seen;
    unsigned rng = static_cast<unsigned>(flip) * 2654435761u + 1;
    bool left = false, right = false;
    int reported = 0;
    for (frame = 0; frame < frames; ++frame) {
      for (const auto& [at, code] : keys)
        if (at == frame) {
          m.key(static_cast<oracle::u8>(code));
          e.key(static_cast<encore::u8>(code));
        }
      if (flip >= 0 && frame > 400) {  // a game played at random, the same for both
        auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
        auto both = [&](int code) { m.key(static_cast<oracle::u8>(code)); e.key(static_cast<encore::u8>(code)); };
        if (random() % 23 == 0) { left = !left; both(left ? 0x2a : 0xaa); }
        if (random() % 23 == 0) { right = !right; both(right ? 0x36 : 0xb6); }
        if (frame % 900 == 0) both(0x1c);
        if (frame % 900 == 5) both(0x9c);
        if (frame % 300 == 100) { both(0xe0); both(0x50); }
        if (frame % 300 == 100 + static_cast<int>(random() % 60) + 20) { both(0xe0); both(0xd0); }
      }
      m.frame();
      e.frame();
      bool differs = false;
      // What only the original's drawing uses is not kept: where it last drew the ball, and
      // the drawing routines' own variables among the code.
      auto drawing = [&](const char* what, unsigned a) {
        if (what[0] == 'd') return (a >= e.A(0x2ef8) && a < e.A(0x2ef8) + 4u) || a == e.A(0x2f08);
        if (what[0] == 'c') return a >= e.F(0x9240) && a < e.F(0x9240) + 0x1a00u;
        return false;
      };
      auto report = [&](const char* what, unsigned a, unsigned ours, unsigned theirs) {
        if (drawing(what, a)) return;
        differs = true;
        if (!seen.insert((static_cast<unsigned>(what[0] + what[9]) << 20) | a).second) return;
        ++reported;
        if (what[0] == 'd') {
          const encore::u16 pl = e.partyLandData(static_cast<encore::u16>(a));
          std::printf("frame %d: data %04x (Party Land's %04x) ours %02x, the original's %02x\n", frame, a, pl, ours, theirs);
        } else {
          std::printf("frame %d: %s %04x ours %02x, the original's %02x\n", frame, what, a, ours, theirs);
        }
      };
      for (unsigned a = 0; a < dataEnd; ++a) {
        const oracle::u8 theirs = m.peek8(ds, static_cast<oracle::u16>(a));
        if (e.memory()[a] != theirs) {
          report("data", a, e.memory()[a], theirs);
          if (all) e.memory()[a] = theirs;
        }
      }
      for (unsigned a = 0; a < codeEnd; ++a) {
        const oracle::u8 theirs = m.peek8(cs, static_cast<oracle::u16>(a));
        if (e.codeMemory()[a] != theirs) {
          report("code variable", a, e.codeMemory()[a], theirs);
          if (all) e.codeMemory()[a] = theirs;
        }
      }
      for (int p = 0; p < 2; ++p)
        for (unsigned a = 0; a < 0x1000; ++a) {
          const oracle::u8 theirs = m.vga.planes[static_cast<std::size_t>(p * 2)][a];
          encore::u8& ours = e.displayMemory()[static_cast<std::size_t>(p)][a];
          if (ours != theirs) {
            report(p ? "display, third plane," : "display, first plane,", a, ours, theirs);
            if (all) ours = theirs;
          }
        }
      for (unsigned a = 0; a < 768; ++a)
        if (e.colours()[a] != m.vga.dac[a]) {
          report("colour byte", a, e.colours()[a], m.vga.dac[a]);
          if (all) e.colours()[a] = m.vga.dac[a];
        }
      if (differs && !all) {
        std::printf("they part at frame %d\n", frame);
        return 1;
      }
      if (reported > 200) {
        std::puts("(and more)");
        return 1;
      }
    }
    std::printf(reported ? "%d places differed over %d frames\n" : "the same for %2$d frames\n", reported, frames);
    return reported ? 1 : 0;
  } catch (const std::exception& ex) {
    std::printf("stopped at frame %d: %s\n", frame, ex.what());
    return 2;
  }
}
