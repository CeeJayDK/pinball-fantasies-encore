// Plays a module of the game's through the driver of ours, into a sound file to listen to:
//   encore-music <the game's folder> <TABLE1.MOD> <seconds> <out.wav> [place in the song]
// Jumps in the music go where the music says, as when no table is listening.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/File.h"
#include "engine/audio/MusicDriver.h"

int main(int argc, char** argv) {
  if (argc < 5) {
    std::puts("usage: encore-music <the game's folder> <module> <seconds> <out.wav> [place]");
    return 2;
  }
  const auto path = pfr::file::findCaseInsensitive(argv[1], argv[2]);
  const auto bytes = path ? pfr::file::readAll(*path) : std::nullopt;
  encore::MusicDriver driver(48000);
  if (!bytes || !driver.load(*bytes)) {
    std::puts("cannot read the module");
    return 2;
  }
  const double seconds = std::atof(argv[3]);
  int jumps = 0;
  driver.onJump = [&](pfr::u8 to) {
    ++jumps;
    return to;
  };
  driver.start();
  if (argc > 5) driver.jump(static_cast<pfr::u16>(std::atoi(argv[5])));
  std::vector<float> sound;
  std::vector<float> chunk(800 * 2);
  for (int frame = 0; frame < static_cast<int>(seconds * 60); ++frame) {
    driver.advance(1.0 / 60);
    driver.render(chunk.data(), 800);
    sound.insert(sound.end(), chunk.begin(), chunk.end());
  }
  std::vector<pfr::u8> wav;
  auto put = [&](pfr::u32 v, int n) {
    for (int i = 0; i < n; ++i) wav.push_back(static_cast<pfr::u8>(v >> (8 * i)));
  };
  const auto size = static_cast<pfr::u32>(sound.size() * 2);
  wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
  put(36 + size, 4);
  wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  put(16, 4); put(1, 2); put(2, 2); put(48000, 4); put(48000 * 4, 4); put(4, 2); put(16, 2);
  wav.insert(wav.end(), {'d', 'a', 't', 'a'});
  put(size, 4);
  float peak = 0;
  for (float s : sound) {
    peak = std::max(peak, s < 0 ? -s : s);
    const float c = s < -1 ? -1 : s > 1 ? 1 : s;
    put(static_cast<pfr::u16>(static_cast<pfr::i16>(c * 32767)), 2);
  }
  pfr::file::writeAll(argv[4], pfr::ByteView(wav.data(), wav.size()));
  std::printf("%.1f seconds, loudest %.2f, %d jumps, at place %d row %d\n", seconds, peak, jumps, driver.position(), driver.row());
  return 0;
}
