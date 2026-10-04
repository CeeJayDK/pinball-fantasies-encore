// encore-trace: what a recorded game does, frame by frame, as text, to compare two engines
// playing the same recording.
//
//   encore-trace <game folder> <file.RPL> [out.txt]   one line per frame
//   encore-trace --diff <a.txt> <b.txt>               the first frame at which they part
//
// A line is: frame, ball x, ball y, layer (0 ground, 1 overhead), score, display. The display
// is a checksum of the dot matrix's 160x16 dots, which carries the scores, messages and
// animations; together with the ball it shows nearly everything a table decides.
#include <cstdio>
#include <fstream>
#include <string>

#include "core/File.h"
#include "table/Replay.h"
#include "table/Table.h"

namespace {

int diff(const char* a, const char* b) {
  std::ifstream fa(a), fb(b);
  if (!fa || !fb) {
    std::puts("cannot read the traces");
    return 2;
  }
  std::string la, lb, pa, pb;
  for (unsigned long n = 0;; ++n) {
    const bool ha = static_cast<bool>(std::getline(fa, la)), hb = static_cast<bool>(std::getline(fb, lb));
    if (!ha && !hb) {
      std::printf("the same: %lu frames\n", n);
      return 0;
    }
    if (!ha || !hb || la != lb) {
      std::printf("they part at line %lu\n", n + 1);
      if (n) std::printf("  before: %s\n", pa.c_str());
      std::printf("  a:      %s\n  b:      %s\n", ha ? la.c_str() : "(ended)", hb ? lb.c_str() : "(ended)");
      return 1;
    }
    pa = la;
    pb = lb;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 4 && std::string(argv[1]) == "--diff") return diff(argv[2], argv[3]);
  if (argc < 3) {
    std::puts("usage: encore-trace <game folder> <file.RPL> [out.txt]\n       encore-trace --diff <a.txt> <b.txt>");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const auto data = pfr::file::readAll(argv[2]);
  const auto rec = data ? pfr::Replay::load(*data) : std::nullopt;
  if (!rec) {
    std::puts("not a recording this version can play");
    return 2;
  }
  const std::string n = std::to_string(rec->table + 1);
  const auto prg = pfr::file::readAll(dir / ("TABLE" + n + ".PRG"));
  const auto mod = pfr::file::readAll(dir / ("TABLE" + n + ".MOD"));
  if (!prg || !mod) {
    std::puts("cannot read the table files");
    return 1;
  }
  std::FILE* out = argc > 3 ? std::fopen(argv[3], "w") : stdout;
  if (!out) {
    std::puts("cannot write the trace");
    return 1;
  }

  pfr::Config config = pfr::Config::defaults();
  config.options = rec->options;
  config.highScores[static_cast<std::size_t>(rec->table)] = rec->highScores;
  pfr::Table t(*prg, *mod, config, rec->table, rec->seed, &rec->carry);
  t.playBack(*rec);
  std::size_t next = 0;
  for (pfr::u32 f = 0; f < rec->frames; ++f) {
    for (; next < rec->events.size() && rec->events[next].frame == f; ++next) {
      const auto& e = rec->events[next];
      if (e.kind != pfr::Replay::Event::Kind::Music)
        t.handleKey(static_cast<pfr::Key>(e.value), e.kind == pfr::Replay::Event::Kind::KeyDown);
    }
    t.runFrame();
    // FNV-1a over the dots
    pfr::u32 dm = 2166136261u;
    for (const auto& row : t.dotMatrix())
      for (bool dot : row) dm = (dm ^ static_cast<pfr::u32>(dot)) * 16777619u;
    std::string score;
    for (pfr::u8 c : t.scoreMain().toAscii())
      if (c != ' ') score += static_cast<char>(c);
    const auto pos = t.ballPos();
    std::fprintf(out, "%u %d %d %d %s %08x\n", f, pos[0], pos[1], t.ballOverhead() ? 1 : 0,
                 score.empty() ? "0" : score.c_str(), dm);
  }
  if (out != stdout) std::fclose(out);
  return 0;
}
