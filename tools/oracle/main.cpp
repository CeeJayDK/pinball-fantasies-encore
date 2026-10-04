// encore-oracle: the referee (docs/own-engine.md). Runs one of the game's own table programs
// in a small machine of ours and says what it does, frame by frame.
//
//   encore-oracle <game folder> <table 1-4> <frames> [--log] [--png <file>]
//                 [--keys <frame>:<scancode>,...]   scancodes in hex, bit 7 set for a key going up
//                 [--peek <seg>:<off>,...]          words of the program's memory, printed every frame
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
    else if (!std::strcmp(argv[i], "--peek") && i + 1 < argc) pairs(argv[++i], 16, peeks);
  }
  oracle::Machine m(argv[1], table, {});
  try {
    m.log = log;
    for (int f = 0; f < frames && !m.exited(); ++f) {
      for (const auto& [at, code] : keys)
        if (at == f) m.key(static_cast<oracle::u8>(code));
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
