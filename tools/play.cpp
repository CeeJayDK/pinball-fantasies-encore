// encore-play: plays a game headlessly on one table with a simple autopilot, printing the
// ball, score and dot matrix, to check the rules and physics without a window; or plays a
// recording again and says whether it comes out the same.
//
//   encore-play <game folder> <table 1-4> [frames] [seed] [out.png]   (ENCORE_RECORD=<file> keeps the game)
//   encore-play <game folder> --replay <file.replay>
//   encore-play <game folder> --verify <file.replay>   (what a server makes of it, as JSON)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/File.h"
#include "core/Png.h"
#include "table/Replay.h"
#include "table/Table.h"

namespace {

void printDm(const pfr::Table& t) {
  const auto& dm = t.dotMatrix();
  for (int y = 0; y < 16; y += 2) {
    std::string line;
    for (int x = 0; x < 160; ++x) {
      const bool a = dm[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      const bool b = dm[static_cast<std::size_t>(y + 1)][static_cast<std::size_t>(x)];
      line += a && b ? "█" : a ? "▀" : b ? "▄" : " ";
    }
    std::printf("  |%s|\n", line.c_str());
  }
}

std::string text(const pfr::Bcd& b) {
  const auto a = b.toAscii();
  std::string s(a.begin(), a.end());
  return s.substr(std::min(s.find_first_not_of(' '), s.size() - 1));
}

/// Plays a recording again; 0 if it came out as it was played.
int replayFile(const std::filesystem::path& dir, const std::filesystem::path& file) {
  const auto data = pfr::file::readAll(file);
  const auto rec = data ? pfr::Replay::load(*data) : std::nullopt;
  if (!rec) {
    std::printf("not a recording this version can play: %s\n", file.string().c_str());
    return 2;
  }
  const std::string n = std::to_string(rec->table + 1);
  const auto prg = pfr::file::readAll(dir / ("TABLE" + n + ".PRG"));
  const auto mod = pfr::file::readAll(dir / ("TABLE" + n + ".MOD"));
  if (!prg || !mod) {
    std::puts("cannot read the table files");
    return 1;
  }
  static constexpr const char* kAngle[] = {"low", "high", "higher"};
  std::printf("table %d, %d balls, angle %s, %u frames (%u:%02u), %zu events\n", rec->table + 1, rec->options.balls,
              kAngle[static_cast<int>(rec->options.angle)], rec->frames, rec->frames / 3600, rec->frames / 60 % 60,
              rec->events.size());
  const auto start = std::chrono::steady_clock::now();
  const pfr::Replay again = pfr::replay(*prg, *mod, *rec);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  for (std::size_t g = 0; g < std::max(rec->games.size(), again.games.size()); ++g) {
    auto line = [](const std::vector<pfr::Replay::Game>& games, std::size_t i) {
      if (i >= games.size()) return std::string("(none)");
      std::string s = "ended at frame " + std::to_string(games[i].endFrame) + (games[i].abandoned ? " (quit)" : "") + ":";
      for (const pfr::Bcd& b : games[i].scores) s += " " + text(b);
      return s;
    };
    std::printf("game %zu\n  played:   %s\n  replayed: %s\n", g + 1, line(rec->games, g).c_str(),
                line(again.games, g).c_str());
  }
  const bool same = again.games == rec->games && again.events == rec->events && again.frames == rec->frames;
  std::printf("%s, in %.0f ms\n", same ? "the same" : "DIFFERENT", ms);
  if (!same && again.events != rec->events) {
    std::size_t i = 0;
    while (i < again.events.size() && i < rec->events.size() && again.events[i] == rec->events[i]) ++i;
    std::printf("events part at #%zu, frame %u\n", i, i < rec->events.size() ? rec->events[i].frame : 0);
  }
  return same ? 0 : 1;
}

/// Checks a recording as the server would, and says so as JSON; 0 if it is believed.
int verifyFile(const std::filesystem::path& dir, const std::filesystem::path& file) {
  const auto data = pfr::file::readAll(file);
  const auto rec = data && data->size() <= 4 * 1024 * 1024 ? pfr::Replay::load(*data) : std::nullopt;
  if (!rec) {
    std::printf("{\"ok\":false,\"reason\":\"not a recording this version can read\"}\n");
    return 1;
  }
  const std::string n = std::to_string(rec->table + 1);
  const auto prg = pfr::file::readAll(dir / ("TABLE" + n + ".PRG"));
  const auto mod = pfr::file::readAll(dir / ("TABLE" + n + ".MOD"));
  if (!prg || !mod) {
    std::fprintf(stderr, "cannot read the table files\n");
    return 2;
  }
  const pfr::Verdict v = pfr::verify(*prg, *mod, *rec);
  static constexpr const char* kAngle[] = {"low", "high", "higher"};
  std::string out = "{\"ok\":" + std::string(v.ok ? "true" : "false");
  if (!v.ok) out += ",\"reason\":\"" + v.reason + "\"";
  out += ",\"format\":" + std::to_string(pfr::Replay::kFormat) + ",\"table\":" + std::to_string(rec->table + 1) +
         ",\"balls\":" + std::to_string(rec->options.balls) + ",\"angle\":\"" +
         kAngle[static_cast<int>(rec->options.angle)] + "\",\"frames\":" + std::to_string(rec->frames);
  if (v.ok) {
    out += ",\"games\":[";
    for (std::size_t g = 0; g < v.replayed.games.size(); ++g) {
      const auto& game = v.replayed.games[g];
      out += std::string(g ? "," : "") + "{\"endFrame\":" + std::to_string(game.endFrame) +
             ",\"abandoned\":" + (game.abandoned ? "true" : "false") + ",\"scores\":[";
      for (std::size_t i = 0; i < game.scores.size(); ++i) out += std::string(i ? "," : "") + text(game.scores[i]);
      out += "]}";
    }
    out += "],\"claimsMatch\":" + std::string(v.claimsMatch ? "true" : "false");
  }
  std::printf("%s}\n", out.c_str());
  return v.ok ? 0 : 1;
}

std::string score(const pfr::Table& t) {
  std::string s;
  for (pfr::u8 c : t.scoreMain().toAscii()) s += static_cast<char>(c);
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::puts("usage: encore-play <game folder> <table 1-4> [frames] [seed] [out.png]\n"
              "       encore-play <game folder> --replay <file.replay>\n"
              "       encore-play <game folder> --verify <file.replay>");
    return 2;
  }
  if (std::string(argv[2]) == "--replay") {
    if (argc < 4) return 2;
    return replayFile(argv[1], argv[3]);
  }
  if (std::string(argv[2]) == "--verify") {
    if (argc < 4) return 2;
    return verifyFile(argv[1], argv[3]);
  }
  const std::filesystem::path dir = argv[1];
  const int table = std::atoi(argv[2]) - 1;
  const int frames = argc > 3 ? std::atoi(argv[3]) : 3000;
  const unsigned seed = argc > 4 ? static_cast<unsigned>(std::atoi(argv[4])) : 1;
  const char* png = argc > 5 ? argv[5] : nullptr;
  const auto prg = pfr::file::readAll(dir / ("TABLE" + std::to_string(table + 1) + ".PRG"));
  const auto mod = pfr::file::readAll(dir / ("TABLE" + std::to_string(table + 1) + ".MOD"));
  if (!prg || !mod) {
    std::puts("cannot read the table files");
    return 1;
  }
  pfr::Config cfg = pfr::Config::defaults();
  if (const char* a = std::getenv("PFR_ANGLE"))
    cfg.options.angle = a[0] == 'l' ? pfr::Angle::Low : a[0] == 'x' ? pfr::Angle::Higher : pfr::Angle::High;
  pfr::Table t(*prg, *mod, cfg, table, seed);
  std::vector<float> audio(960 * 2);
  std::vector<std::string> events;
  t.trace = &events;

  using pfr::Key;
  int plungeAt = -1;
  bool left = false, right = false;
  std::string lastScore;
  int lastBall = 0;
  int prevY = 0;
  int flipAt = -1, lowestAfterFlip = 999, shots = 0;
  long sumTop = 0;
  for (int f = 0; f < frames; ++f) {
    if (f == 30) t.handleKey(Key::Enter, true), t.handleKey(Key::Enter, false);
    const auto pos = t.ballPos();
    // Plunge whenever the ball rests at the spring.
    if (!t.inAttract() && pos[0] >= 290 && pos[1] >= 515 && plungeAt < 0) {
      plungeAt = f;
      t.handleKey(Key::ArrowDown, true);
    }
    if (plungeAt >= 0 && f == plungeAt + 40) t.handleKey(Key::ArrowDown, false);
    if (plungeAt >= 0 && f > plungeAt + 200 && pos[1] < 500) plungeAt = -1;
    // Autopilot: flip when the ball is above a flipper.
    // Flip while the ball falls onto a flipper; never hold, so it is not cradled forever.
    const bool falling = pos[1] > prevY;
    prevY = pos[1];
    const bool zone = falling && pos[1] > 485 && pos[1] < 545;
    const bool wantL = zone && pos[0] < 150;
    const bool wantR = zone && pos[0] >= 130 && pos[0] < 290;
    // Measure each flipper shot: the highest point the ball reaches within 3 seconds.
    if ((wantL && !left) || (wantR && !right)) {
      if (flipAt >= 0 && lowestAfterFlip < 400) { sumTop += lowestAfterFlip; ++shots; }
      flipAt = f;
      lowestAfterFlip = 999;
    }
    if (flipAt >= 0 && f - flipAt < 180) lowestAfterFlip = std::min<int>(lowestAfterFlip, pos[1]);
    if (wantL != left) t.handleKey(Key::ShiftLeft, left = wantL);
    if (wantR != right) t.handleKey(Key::ShiftRight, right = wantR);

    t.runFrame();
    for (const auto& e : events) std::printf("frame %5d: %s\n", f, e.c_str());
    events.clear();
    t.player().render(audio.data(), 800);  // keep the music sequencer moving (48000/60)

    const std::string s = score(t);
    if (t.currentBall() != lastBall) {
      std::printf("frame %5d: ball %d\n", f, t.currentBall());
      lastBall = t.currentBall();
    }
    if (f % (std::getenv("PFR_EVERY") ? std::atoi(std::getenv("PFR_EVERY")) : 250) == 0 || (s != lastScore && f % 25 == 0)) {
      std::printf("frame %5d: pos (%3d,%3d) %s score %s%s\n", f, pos[0], pos[1], t.ballOverhead() ? "ramp" : "    ",
                  s.c_str(), t.inAttract() ? " [attract]" : "");
      lastScore = s;
    }
    if (std::getenv("ENCORE_DM") && f % 500 == 499) printDm(t);
  }
  if (std::getenv("ENCORE_DM")) printDm(t);
  if (const char* out = std::getenv("ENCORE_RECORD")) pfr::file::writeAll(out, t.recording().save());
  if (shots) std::printf("flipper shots reaching the upper table: %d, average top y %ld\n", shots, sumTop / shots);
  if (png) {
    std::vector<pfr::u8> pixels(320 * static_cast<std::size_t>(t.screenHeight()));
    std::vector<pfr::Rgb> pal(256);
    t.render(pixels.data(), pal.data());
    pfr::writeIndexedPng(png, pixels.data(), 320, t.screenHeight(), pal);
  }
  return 0;
}
