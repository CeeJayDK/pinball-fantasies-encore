// encore-front: the slides and the menu with no window: frames of it as pictures, to look at.
//   encore-front <the game's folder> <frames> <out prefix> [--menu <table 1-4>] [--keys <frame>:<key>,...] [--every <n>]
// Keys: space, enter, esc, up, down, f1 to f5.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/File.h"
#include "core/Png.h"
#include "engine/game/Front.h"

using namespace encore;

int main(int argc, char** argv) {
  if (argc < 4) {
    std::puts("usage: encore-front <the game's folder> <frames> <out prefix> [--menu <table>] [--keys <frame>:<key>,...] [--every <n>]");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const int frames = std::atoi(argv[2]);
  const std::string prefix = argv[3];
  int from = -1, every = 0;
  std::multimap<int, Key> keys;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--menu") && i + 1 < argc) from = std::atoi(argv[++i]) - 1;
    else if (!std::strcmp(argv[i], "--every") && i + 1 < argc) every = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) {
      std::string list = argv[++i];
      for (std::size_t at = 0; at < list.size();) {
        const std::size_t end = std::min(list.find(',', at), list.size());
        const std::string one = list.substr(at, end - at);
        const std::size_t colon = one.find(':');
        const std::string name = one.substr(colon + 1);
        const Key k = name == "space" ? Key::Space : name == "enter" ? Key::Enter : name == "esc" ? Key::Escape : name == "up" ? Key::ArrowUp
                    : name == "down" ? Key::ArrowDown : static_cast<Key>(static_cast<int>(Key::F1) + (name[1] - '1'));
        keys.emplace(std::atoi(one.c_str()), k);
        at = end + 1;
      }
    }
  }
  auto read = [&](const char* name) {
    const auto path = file::findCaseInsensitive(dir, name);
    return path ? file::readAll(*path).value_or(Bytes{}) : Bytes{};
  };
  const Bytes prg = read("INTRO.PRG"), mod = read(from < 0 ? "INTRO.MOD" : "MOD2.MOD");
  Front front(prg, mod, Config::defaults(), from);
  std::vector<u8> pixels(640 * 480);
  std::vector<Rgb> colours(256);
  for (int f = 0; f < frames; ++f) {
    for (auto [it, end] = keys.equal_range(f); it != end; ++it) front.key(it->second, true);
    const Front::Action a = front.frame();
    front.noSound();
    if (a.kind != Front::Action::Kind::None) std::printf("frame %d: action %d, table %d\n", f, static_cast<int>(a.kind), a.table + 1);
    if ((every && f % every == every - 1) || f == frames - 1) {
      front.draw(pixels.data(), colours.data());
      writeIndexedPng(prefix + std::to_string(f + 1) + ".png", pixels.data(), 640, 480, colours);
    }
  }
  return 0;
}
