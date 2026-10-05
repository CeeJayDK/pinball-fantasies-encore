// encore-oracle-table: the engine's memory against the original's, frame by frame.
//
// The referee starts a table and runs its start-up; the engine is given the memory as that
// leaves it. Both are then given the same keys, and after every frame the engine's data
// segment, the variables among its code and its colours are compared with the original's.
//
//   encore-oracle-table <game folder> <table 1-4> <frames> [--keys <frame>:<scancode>,...]
//                       [--flip <seed>]  plays at random from frame 400 on
//                       [--wild]   with --flip: nudges, letters and more players too
//                       [--options 001010]  balls, angle, scrolling, music off, resolution, mono
//                       [--start]  the engine starts by itself, and that is compared first
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
  bool ownStart = false, wild = false;
  oracle::Machine::Config config;
  encore::Engine::Options options;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--all")) all = true;
    else if (!std::strcmp(argv[i], "--start")) ownStart = true;
    else if (!std::strcmp(argv[i], "--wild")) wild = true;
    else if (!std::strcmp(argv[i], "--options") && i + 1 < argc) {  // six digits, as PINBALL.CFG's bytes
      const char* o = argv[++i];
      for (int k = 0; k < 6 && o[k]; ++k) config.bytes[k] = static_cast<oracle::u8>(o[k] - '0');
      options = {config.bytes[0] != 0, config.bytes[1] != 0, config.bytes[2], config.bytes[3] != 0, config.bytes[4] != 0, config.bytes[5] != 0};
      ownStart = true;
    }
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
    oracle::Machine m(dir, table, config);
    m.setLoop(0x10, e.F(encore::at::mainLoop));
    m.loopsPerFrame = e.loopsPerFrame;
    m.boot();
    if (const char* w = std::getenv("ENCORE_WATCH")) e.debugWatch = static_cast<int>(std::strtol(w, nullptr, 16));
    if (const char* w = std::getenv("ENCORE_WATCH")) m.watchWrite = (oracle::u32{m.seg(e.dataSegment())} << 4) + static_cast<oracle::u32>(std::strtol(w, nullptr, 16));
    const oracle::u16 ds = m.seg(e.dataSegment()), cs = m.seg(0x10);
    if (ownStart) {
      // The engine starts by itself, and what that leaves is compared with what the original's
      // start-up left, before it is taken over like every other difference.
      e.start(options);
      int differ = 0;
      unsigned lastAt = 0xfffffff;
      const char* lastWhat = "";
      auto say = [&](const char* what, unsigned a, unsigned ours, unsigned theirs) {
        // what the start-up leaves that is nothing to the engine: the sound driver's file
        // name, and the variables of the routines that draw
        if (!std::strcmp(what, "data") && a >= e.A(0x22c5) && a < e.A(0x22c5) + 13u) return;
        if (what[0] == 'c' && a >= e.F(0x9240) && a < e.F(0x9240) + 0x1a00u) return;
        ++differ;
        // only the first of a run is said
        if (!(lastWhat == what && a == lastAt + 1) && differ <= 400)
          std::printf("after start-up: %s %04x ours %02x, the original's %02x\n", what, a, ours, theirs);
        lastAt = a;
        lastWhat = what;
      };
      for (unsigned a = 0; a < 0x6c00; ++a)
        if (e.memory()[a] != m.peek8(ds, static_cast<oracle::u16>(a))) say("data", a, e.memory()[a], m.peek8(ds, static_cast<oracle::u16>(a)));
      for (unsigned a = 0; a < 0xac00; ++a)
        if (e.codeMemory()[a] != m.peek8(cs, static_cast<oracle::u16>(a))) say("code variable", a, e.codeMemory()[a], m.peek8(cs, static_cast<oracle::u16>(a)));
      for (std::size_t a = 0x100; a + 0x10000 < e.image().size(); ++a) {
        const oracle::u8 theirs = m.peek8(static_cast<oracle::u16>(encore::Program::kLoadSegment + (a >> 4)), static_cast<oracle::u16>(a & 15));
        const std::size_t dataAt = std::size_t{e.dataSegment()} * 16, codeAt = 0x100;
        if ((a >= dataAt && a < dataAt + 0x10000) || (a >= codeAt && a < codeAt + 0xac00)) continue;  // those two are kept apart
        if (e.image()[a] != theirs) say("the program's other segments, byte", static_cast<unsigned>(a), e.image()[a], theirs);
      }
      for (std::size_t p = 0; p < 4; ++p)
        for (unsigned a = 0; a < 0x10000; ++a)
          if ((a < 0x0ad4 || a >= 0xc7d4 || (a - 0x0ad4) % 0x54 >= 0x50) && e.videoMemory()[p][a] != m.vga.planes[p][a])
            say("video memory", a, e.videoMemory()[p][a], m.vga.planes[p][a]);
      for (unsigned a = 0; a < 768; ++a)
        if (e.colours()[a] != m.vga.dac[a]) say("colour byte", a, e.colours()[a], m.vga.dac[a]);
      std::printf(differ ? "after start-up: %d bytes differ\n" : "after start-up: the same\n", differ);
    }
    for (unsigned a = 0; a < 0x10000; ++a) {
      e.memory()[a] = m.peek8(ds, static_cast<oracle::u16>(a));
      e.codeMemory()[a] = m.peek8(cs, static_cast<oracle::u16>(a));
    }
    for (std::size_t a = 0; a < e.image().size() && a < 0x90000; ++a)
      e.image()[a] = m.peek8(static_cast<oracle::u16>(encore::Program::kLoadSegment + (a >> 4)), static_cast<oracle::u16>(a & 15));
    e.colours() = m.vga.dac;
    e.videoMemory() = m.vga.planes;

    // The data segment ends where the next begins; the code's variables are among its code.
    const unsigned dataEnd = 0x6c00, codeEnd = 0xac00;
    std::set<unsigned> seen;
    unsigned rng = static_cast<unsigned>(flip) * 2654435761u + 1;
    bool left = false, right = false;
    int reported = 0;
    long videoDiffers = 0;
    for (frame = 0; frame < frames; ++frame) {
      for (const auto& [at, code] : keys)
        if (at == frame) {
          m.key(static_cast<oracle::u8>(code));
          e.key(static_cast<encore::u8>(code));
        }
      if (flip >= 0 && frame > 400) {  // a game played at random, the same for both
        auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
        auto both = [&](int code) {
          if (std::getenv("ENCORE_KEYS")) std::printf("key %02x at frame %d\n", code, frame);
          m.key(static_cast<oracle::u8>(code));
          e.key(static_cast<encore::u8>(code));
        };
        if (random() % 23 == 0) { left = !left; both(left ? 0x2a : 0xaa); }
        if (random() % 23 == 0) { right = !right; both(right ? 0x36 : 0xb6); }
        if (frame % 900 == 0) both(0x1c);
        if (frame % 900 == 5) both(0x9c);
        if (frame % 300 == 100) { both(0xe0); both(0x50); }
        if (frame % 300 == 100 + static_cast<int>(random() % 60) + 20) { both(0xe0); both(0xd0); }
        if (wild) {  // and the rest of the keyboard: nudges, letters, more players
          if (random() % 500 == 0) both(0x39);
          if (random() % 500 == 1) both(0xb9);
          if (random() % 700 == 0) { const int letter = 0x10 + static_cast<int>(random() % 35); both(letter); both(letter | 0x80); }
          if (random() % 2500 == 0) { both(0x1c); both(0x9c); }
          if (random() % 4000 == 0) { const int f = 0x3b + static_cast<int>(random() % 4); both(f); both(f | 0x80); }
        }
      }
      m.frame();
      e.frame();
      bool differs = false;
      // What only the original's drawing uses is not kept: where it last drew the ball, and
      // the drawing routines' own variables among the code.
      auto drawing = [&](const char* what, unsigned a) {
        if (!std::strcmp(what, "data")) {
          if ((a >= e.A(0x2ef8) && a < e.A(0x2ef8) + 4u) || a == e.A(0x2f08)) return true;
          // each flipper's record keeps which picture of it was last drawn, and where
          for (unsigned f = e.A(0x6950); f < e.A(0x6950) + 3 * 0x3cu; f += 0x3c)
            if ((a >= f + 0x2c && a < f + 0x2e) || (a >= f + 0x34 && a < f + 0x36)) return true;
          return false;
        }
        if (what[0] == 'c') return a >= e.F(0x9240) && a < e.F(0x9240) + 0x1a00u;
        return false;
      };
      auto report = [&](const char* what, unsigned a, unsigned ours, unsigned theirs) {
        if (drawing(what, a)) return;
        differs = true;
        if (!seen.insert((static_cast<unsigned>(what[0] * 31 + what[1] * 7 + what[std::strlen(what) - 2]) << 20) | a).second) return;
        ++reported;
        if (std::getenv("ENCORE_VRAM")) {
          const unsigned base = m.peek16(ds, e.A(0x239f));
          int bad = 0, first = -1;
          for (unsigned si = 0; si < 20400; ++si)
            if (m.vga.planes[si & 3][(base + (si >> 2)) & 0xffff] != m.peek8(m.seg(0x7734), static_cast<oracle::u16>(si))) {
              ++bad;
              if (first < 0) first = static_cast<int>(si);
            }
          std::printf("  (the copy of the ramps' marks in video memory at %04x: %d bytes differ, the first at row %d column %d)\n", base, bad,
                      first / 40, first % 40 * 8);
        }
        if (std::getenv("ENCORE_BALL"))
          std::printf("  (trigger %04x, ours %04x) (ball %d,%d layer %02x nudge %d)\n", m.peek16(ds, e.A(0x3316)), static_cast<unsigned>(e.W(0x3316)), m.peek16(ds, e.A(encore::at::ballX)), m.peek16(ds, e.A(encore::at::ballY)),
                      m.peek8(ds, e.A(encore::at::layer)), m.peek16(ds, e.A(encore::at::nudgeLift)));
        if (!std::strcmp(what, "data")) {
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
        for (unsigned a = 0; a < 0x0ad4; ++a) {  // up to where the table's picture begins
          const oracle::u8 theirs = m.vga.planes[static_cast<std::size_t>(p * 2)][a];
          encore::u8& ours = e.videoMemory()[static_cast<std::size_t>(p * 2)][a];
          if (ours != theirs) {
            report(p ? "display, third plane," : "display, first plane,", a, ours, theirs);
            if (all) ours = theirs;
          }
        }
      // past the picture: the mask copies and the pictures kept over them
      for (std::size_t p = 0; p < 4; ++p)
        for (unsigned a = 0xc7d4; a < 0x10000; ++a)
          if (e.videoMemory()[p][a] != m.vga.planes[p][a]) {
            // The original draws the ball there as it leaves the bottom of the table, over the
            // first rows of a mask's copy, and puts back what was there when the ball moves on.
            // The engine does not draw. The bytes are counted, said only if asked
            // (ENCORE_VIDEO), and left as the engine has them unless ENCORE_TAKE_VIDEO is set.
            ++videoDiffers;
            if (std::getenv("ENCORE_VIDEO")) report("video memory past the picture,", a, e.videoMemory()[p][a], m.vga.planes[p][a]);
            if (std::getenv("ENCORE_TAKE_VIDEO")) e.videoMemory()[p][a] = m.vga.planes[p][a];
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
    if (videoDiffers) std::printf("(the original's drawing past the picture: %ld bytes, frame by frame)\n", videoDiffers);
    return reported ? 1 : 0;
  } catch (const std::exception& ex) {
    std::printf("stopped at frame %d: %s\n", frame, ex.what());
    return 2;
  }
}
