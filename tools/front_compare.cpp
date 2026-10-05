// A passing tool: the slides and menu of the engine being replaced and of ours, given the
// same keys, their screens compared frame by frame. It goes when the old engine does.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/File.h"
#include "core/Png.h"
#include "engine/game/Front.h"
#include "intro/Intro.h"

using namespace encore;

int main(int argc, char** argv) {
  if (argc < 4) return 2;
  const std::filesystem::path dir = argv[1];
  const int frames = std::atoi(argv[2]);
  const std::string prefix = argv[3];
  int from = -1, shift = 0;
  bool wantHd = false;
  std::multimap<int, Key> keys;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--menu") && i + 1 < argc) from = std::atoi(argv[++i]) - 1;
    else if (!std::strcmp(argv[i], "--shift") && i + 1 < argc) shift = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--hd")) wantHd = true;
    else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) {
      std::string list = argv[++i];
      for (std::size_t at = 0; at < list.size();) {
        const std::size_t end = std::min(list.find(',', at), list.size());
        const std::string one = list.substr(at, end - at);
        const std::string name = one.substr(one.find(':') + 1);
        const Key k = name == "space" ? Key::Space : name == "enter" ? Key::Enter : name == "esc" ? Key::Escape : name == "up" ? Key::ArrowUp
                    : name == "down" ? Key::ArrowDown : static_cast<Key>(static_cast<int>(Key::F1) + (name[1] - '1'));
        keys.emplace(std::atoi(one.c_str()), k);
        at = end + 1;
      }
    }
  }
  auto read = [&](const char* name) { return file::readAll(*file::findCaseInsensitive(dir, name)).value_or(Bytes{}); };
  const Bytes prg = read("INTRO.PRG"), mod = read(from < 0 ? "INTRO.MOD" : "MOD2.MOD");
  const Config config = Config::defaults();
  Front ours(prg, mod, config, from);
  pfr::Intro theirs(prg, mod, config, from);
  std::vector<u8> a(640 * 480), b(640 * 480 * 4);
  std::vector<Rgb> ca(256), cb(256);
  std::vector<float> sound(1600);
  HdFrame ha, hb;
  int shown = 0;
  long differing = 0;
  // `shift`: ours is that many frames ahead of theirs (theirs runs that many first)
  for (int f = 0; f < shift; ++f) {
    theirs.runFrame();
    theirs.player().render(sound.data(), 800);
  }
  for (int f = 0; f < frames; ++f) {
    for (auto [it, end] = keys.equal_range(f); it != end; ++it) {
      ours.key(it->second, true);
      theirs.handleKey(it->second, true);
      theirs.handleKey(it->second, false);
    }
    ours.frame();
    ours.noSound();
    theirs.runFrame();
    theirs.player().render(sound.data(), 800);
    ours.draw(a.data(), ca.data(), wantHd ? &ha : nullptr);
    const int w = theirs.width(), h = theirs.height();
    theirs.render(b.data(), cb.data(), wantHd ? &hb : nullptr);
    long diff = 0, hdDiff = 0;
    if (w == 640 && h == 480) {
      const bool panel = std::getenv("NO_PANEL") != nullptr;  // leave out the panel's upper half
      for (std::size_t i = 0; i < a.size(); ++i) {
        if (panel && i % 640 < 128 && i / 640 < 230) continue;
        const Rgb x = ca[a[i]], y = cb[b[i]];
        diff += std::abs(x.r - y.r) > 4 || std::abs(x.g - y.g) > 4 || std::abs(x.b - y.b) > 4;
      }
      if (wantHd)
        for (std::size_t i = 0; i < ha.map.size() && i < hb.map.size(); ++i)
          hdDiff += ha.map[i].picture != hb.map[i].picture || (ha.map[i].picture != 0 && (ha.map[i].x8 != hb.map[i].x8 || ha.map[i].y8 != hb.map[i].y8));
    } else {
      diff = -1;
    }
    if (diff != 0 || hdDiff != 0) {
      ++differing;
      if (shown < 12 || f % 200 == 0 || (std::getenv("DUMP") && std::atoi(std::getenv("DUMP")) == f)) {
        std::printf("frame %d: theirs %dx%d, %ld dots differ, %ld of the HD map\n", f, w, h, diff, hdDiff);
        const char* dump = std::getenv("DUMP");
        if (dump && std::atoi(dump) == f && wantHd) {
          int x0 = 640, y0 = 480, x1 = -1, y1 = -1, said = 0;
          for (int y = 0; y < 480; ++y)
            for (int x = 0; x < 640; ++x) {
              const HdPixel& p = ha.map[static_cast<std::size_t>(y * 640 + x)];
              const HdPixel& q = hb.map[static_cast<std::size_t>(y * 640 + x)];
              if (p.picture == q.picture && (p.picture == 0 || (p.x8 == q.x8 && p.y8 == q.y8))) continue;
              x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
              if (said++ < 6 || (said % 3000 == 0))
                std::printf("  (%d,%d): ours %03x %d,%d  theirs %03x %d,%d\n", x, y, p.picture, p.x8, p.y8, q.picture, q.x8, q.y8);
            }
          std::printf("  HD differs within (%d,%d)-(%d,%d)\n", x0, y0, x1, y1);
        }
        if (((!dump && shown < 6) || (dump && std::atoi(dump) == f)) && w == 640 && h == 480) {
          writeIndexedPng(prefix + std::to_string(f) + "-ours.png", a.data(), 640, 480, ca);
          writeIndexedPng(prefix + std::to_string(f) + "-theirs.png", b.data(), w, h, cb);
        }
        ++shown;
      }
    }
    if (wantHd && f == frames - 1) {
      std::map<int, std::array<int, 6>> boxes;  // per picture: screen box and the picture's corner there
      for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 640; ++x) {
          const HdPixel& p = hb.map[static_cast<std::size_t>(y * 640 + x)];
          if ((p.picture & 0xff) == 0) continue;
          auto [it, fresh] = boxes.try_emplace(p.picture & 0x3ff, std::array<int, 6>{x, y, x, y, p.x8, p.y8});
          auto& bx = it->second;
          bx[2] = std::max(bx[2], x), bx[3] = std::max(bx[3], y);
          (void)fresh;
        }
      for (const auto& [id, bx] : boxes)
        std::printf("theirs: picture %03x on screen (%d,%d)-(%d,%d), its (%d,%d)/8 at the first; size %dx%d\n", id, bx[0], bx[1], bx[2], bx[3],
                    bx[4], bx[5], hb.size[static_cast<std::size_t>(id & 0xff)][0], hb.size[static_cast<std::size_t>(id & 0xff)][1]);
    }
  }
  std::printf("%ld of %d frames differ\n", differing, frames);
  return 0;
}
