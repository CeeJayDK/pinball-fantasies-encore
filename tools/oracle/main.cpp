// encore-oracle: the referee (docs/own-engine.md). Runs one of the game's own table programs
// in a small machine of ours and says what it does, frame by frame.
//
//   encore-oracle <game folder> <table 1-4> <frames> [--log] [--png <file> ]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--log")) log = true;
    else if (!std::strcmp(argv[i], "--png") && i + 1 < argc) png = argv[++i];
  }
  oracle::Machine m(argv[1], table, {});
  try {
    m.log = log;
    for (int f = 0; f < frames && !m.exited(); ++f) m.frame();
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
