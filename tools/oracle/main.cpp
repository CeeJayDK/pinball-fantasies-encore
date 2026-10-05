// encore-oracle: the referee (docs/own-engine.md). Runs one of the game's own table programs
// in a small machine of ours and says what it does, frame by frame.
//
//   encore-oracle <game folder> <table 1-4> <frames> [--log] [--png <file>]
//                 [--keys <frame>:<scancode>,...]   scancodes in hex, bit 7 set for a key going up
//                 [--peek <seg>:<off>,...]          words of the program's memory, printed every frame
//                 [--coverage <file>]               adds where instructions ran in the code segment to a file
//                 [--flip <seed>]                   presses the flippers at random from then on
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Machine.h"
#include "core/Png.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::puts("usage: encore-oracle <game folder> <table 1-4> <frames> [--log] [--png <file>]");
    return 2;
  }
  const int table = std::atoi(argv[2]) - 1;
  const int frames = std::atoi(argv[3]);
  bool log = false;
  const char* png = nullptr;
  const char* coverageFile = nullptr;
  int flip = -1;
  std::vector<std::pair<int, int>> keys, peeks;
  auto pairs = [](const char* text, int base1, std::vector<std::pair<int, int>>& out) {
    for (const char* p = text; *p;) {
      char* end = nullptr;
      const long a = std::strtol(p, &end, base1);
      if (*end != ':') break;
      const long b = std::strtol(end + 1, &end, 16);
      out.push_back({static_cast<int>(a), static_cast<int>(b)});
      p = *end == ',' ? end + 1 : end;
    }
  };
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--log")) log = true;
    else if (!std::strcmp(argv[i], "--png") && i + 1 < argc) png = argv[++i];
    else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) pairs(argv[++i], 10, keys);
    else if (!std::strcmp(argv[i], "--coverage") && i + 1 < argc) coverageFile = argv[++i];
    else if (!std::strcmp(argv[i], "--flip") && i + 1 < argc) flip = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--peek") && i + 1 < argc) pairs(argv[++i], 16, peeks);
  }
  oracle::Machine m(argv[1], table, {});
  std::vector<bool> coverage(0x10000, false);
  if (coverageFile) {
    if (std::FILE* in = std::fopen(coverageFile, "r")) {
      unsigned a = 0;
      while (std::fscanf(in, "%x", &a) == 1) coverage[a & 0xffff] = true;
      std::fclose(in);
    }
    m.cpu.coverage = &coverage;
    m.cpu.coverageSegment = m.seg(0x10);
  }
  unsigned rng = static_cast<unsigned>(flip) * 2654435761u + 1;
  bool left = false, right = false;
  try {
    m.log = log;
    for (int f = 0; f < frames && !m.exited(); ++f) {
      for (const auto& [at, code] : keys)
        if (at == f) m.key(static_cast<oracle::u8>(code));
      if (flip >= 0 && f > 400) {
        auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
        if (random() % 23 == 0) { left = !left; m.key(left ? 0x2a : 0xaa); }
        if (random() % 23 == 0) { right = !right; m.key(right ? 0x36 : 0xb6); }
        // and keeps the game going: enter now and then, the plunger pulled and let go
        if (f % 900 == 0) m.key(0x1c);
        if (f % 900 == 5) m.key(0x9c);
        if (f % 300 == 100) { m.key(0xe0); m.key(0x50); }
        if (f % 300 == 100 + static_cast<int>(random() % 60) + 20) { m.key(0xe0); m.key(0xd0); }
        if (random() % 400 == 0) m.key(0x39);
        if (random() % 400 == 1) m.key(0xb9);
      }
      m.frame();
      if (!peeks.empty()) {
        std::printf("%d", f);
        for (const auto& [seg, off] : peeks)
          std::printf(" %d", static_cast<oracle::i16>(m.peek16(m.seg(static_cast<oracle::u16>(seg)), static_cast<oracle::u16>(off))));
        std::printf("\n");
      }
    }
    std::printf("crtc: start %04x pitch %02x split %d scan %02x mode %02x seq4 %02x\n", m.vga.startAddress(), m.vga.crtc[0x13],
                m.vga.lineCompare(), m.vga.crtc[9], m.vga.gc[5], m.vga.seq[4]);
    std::printf("%u frames, %llu instructions, at %04x:%04x\n", m.frames, static_cast<unsigned long long>(m.cpu.executed),
                m.cpu.s[oracle::Cpu::CS], m.cpu.ip);
    if (coverageFile) {
      if (std::FILE* out = std::fopen(coverageFile, "w")) {
        for (unsigned a = 0; a < 0x10000; ++a)
          if (coverage[a]) std::fprintf(out, "%04x\n", a);
        std::fclose(out);
      }
    }
    if (png) {
      const int height = 350;
      const auto pic = m.vga.picture(height);
      pfr::Bytes rgb(static_cast<std::size_t>(320 * height * 3));
      for (std::size_t i = 0; i < pic.size(); ++i)
        for (std::size_t c = 0; c < 3; ++c) rgb[i * 3 + c] = static_cast<pfr::u8>(m.vga.dac[pic[i] * 3u + c] * 255 / 63);
      pfr::writeRgbPng(png, rgb.data(), 320, height);
    }
  } catch (const std::exception& e) {
    std::printf("stopped at frame %u: %s\n%s\n", m.frames, e.what(), m.cpu.history().c_str());
    return 1;
  }
  return 0;
}
