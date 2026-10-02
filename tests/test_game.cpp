// Tests against the real game files. They look for the supported version in ENCORE_DATA, or
// in the FANTASY folder beside the project, and are skipped when neither is present.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Test.h"
#include "assets/TableAssets.h"
#include "core/File.h"
#include "data/GameVersion.h"
#include "intro/Intro.h"
#include "table/Replay.h"
#include "table/Table.h"

using namespace pfr;

namespace {

std::filesystem::path dataDir() {
  if (const char* d = std::getenv("ENCORE_DATA")) return d;
  return std::filesystem::path(ENCORE_SOURCE_DIR) / ".." / ".." / "FANTASY";
}

bool haveData() {
  static const bool ok = std::filesystem::exists(dataDir()) && unsupportedGameFiles(dataDir()).empty();
  if (!ok) std::printf("  (skipped: no supported game files at %s)\n", dataDir().string().c_str());
  return ok;
}

Bytes read(const std::string& name) { return file::readAll(dataDir() / name).value_or(Bytes{}); }

/// Plays a scripted game: start, plunge each ball, flip when the ball falls on a flipper.
struct Outcome {
  int balls = 0;
  bool backToAttract = false;
  Bcd best;
  int triggers = 0;
};
Outcome play(int table, int frames, u64 seed, Replay* recording = nullptr) {
  Table t(read("TABLE" + std::to_string(table + 1) + ".PRG"), read("TABLE" + std::to_string(table + 1) + ".MOD"),
          Config::defaults(), table, seed);
  std::vector<std::string> events;
  t.trace = &events;
  std::vector<float> audio(1600);
  Outcome o;
  int plungeAt = -1, prevY = 0;
  bool l = false, r = false, started = false;
  for (int f = 0; f < frames; ++f) {
    if (f == 30) t.handleKey(Key::Enter, true), t.handleKey(Key::Enter, false);
    const auto p = t.ballPos();
    if (!t.inAttract() && p[0] >= 290 && p[1] >= 515 && plungeAt < 0) {
      plungeAt = f;
      t.handleKey(Key::ArrowDown, true);
    }
    if (plungeAt >= 0 && f == plungeAt + 40) t.handleKey(Key::ArrowDown, false);
    if (plungeAt >= 0 && f > plungeAt + 200 && p[1] < 500) plungeAt = -1;
    const bool zone = p[1] > prevY && p[1] > 485 && p[1] < 545;
    prevY = p[1];
    const bool wl = zone && p[0] < 150, wr = zone && p[0] >= 130 && p[0] < 290;
    if (wl != l) t.handleKey(Key::ShiftLeft, l = wl);
    if (wr != r) t.handleKey(Key::ShiftRight, r = wr);
    t.runFrame();
    t.player().render(audio.data(), 800);
    o.balls = std::max<int>(o.balls, t.currentBall());
    if (t.scoreMain() > o.best) o.best = t.scoreMain();
    if (!t.inAttract()) started = true;
    if (started && t.inAttract()) o.backToAttract = true;
    o.triggers += static_cast<int>(events.size());
    events.clear();
  }
  if (recording) *recording = t.recording();
  return o;
}

}  // namespace

TEST(assets_load_all_tables) {
  if (!haveData()) return;
  for (int t = 0; t < 4; ++t) {
    const TableAssets a = TableAssets::load(read("TABLE" + std::to_string(t + 1) + ".PRG"), t);
    CHECK(a.ballOutline.size() == 44);
    CHECK(!a.scripts.empty());
    CHECK(a.scriptBinds[static_cast<std::size_t>(ScriptBind::Main)].has_value());
    CHECK(a.jingleBinds[static_cast<std::size_t>(JingleBind::Main)].has_value());
    CHECK(!a.rollTriggers[0].empty());
  }
}

TEST(intro_loads) {
  if (!haveData()) return;
  Intro intro(read("INTRO.PRG"), read("INTRO.MOD"), Config::defaults(), -1);
  std::vector<u8> pixels(640 * 480);
  std::vector<Rgb> pal(256);
  for (int f = 0; f < 60; ++f) intro.runFrame();
  intro.render(pixels.data(), pal.data());
  CHECK(pal[1].r + pal[1].g + pal[1].b > 0 || pal[2].r + pal[2].g + pal[2].b > 0);
}

// Every table plays a full game of three balls and returns to attract mode, scoring and
// firing its rules on the way.
TEST(every_table_plays_a_game) {
  if (!haveData()) return;
  for (int t = 0; t < 4; ++t) {
    const Outcome o = play(t, 20000, 3);
    const auto best = o.best.toAscii();
    std::printf("  table %d: %d balls, best score %s, %d triggers\n", t + 1, o.balls,
                std::string(best.begin(), best.end()).c_str(), o.triggers);
    CHECK(o.balls == 3);
    CHECK(o.backToAttract);
    CHECK(!o.best.isZero());
    CHECK(o.triggers > 10);
  }
}

// The same seed and input give the same game.
TEST(games_are_deterministic) {
  if (!haveData()) return;
  const Outcome a = play(3, 3000, 42), b = play(3, 3000, 42);
  CHECK(a.best == b.best);
  CHECK(a.triggers == b.triggers);
}

// A game recorded while it was played, music and all, plays again from the file with no
// sound at all and arrives at the same games, scores and events, frame for frame.
TEST(recorded_games_replay_exactly) {
  if (!haveData()) return;
  for (int t = 0; t < 4; ++t) {
    Replay played;
    play(t, 20000, 11 + static_cast<u64>(t), &played);
    const Bytes file = played.save();
    const auto loaded = Replay::load(file);
    CHECK(loaded.has_value());
    if (!loaded) continue;
    CHECK(loaded->events == played.events);
    CHECK(loaded->games == played.games);
    const std::string n = std::to_string(t + 1);
    const Replay again = replay(read("TABLE" + n + ".PRG"), read("TABLE" + n + ".MOD"), *loaded);
    std::size_t music = 0;
    for (const auto& e : played.events) music += e.kind == Replay::Event::Kind::Music;
    const auto score = played.games.empty() ? Bcd::kZero.toAscii() : played.games[0].scores[0].toAscii();
    std::printf("  table %d: %zu bytes, %zu events (%zu music), %zu games, first %s; replayed %s\n", t + 1,
                file.size(), played.events.size(), music, played.games.size(),
                std::string(score.begin(), score.end()).c_str(),
                again.events == played.events && again.games == played.games ? "the same" : "DIFFERENT");
    CHECK(!played.games.empty());
    CHECK(again.frames == played.frames);
    CHECK(again.events == played.events);
    CHECK(again.games == played.games);
  }
}

// A game played with a cheat that makes it easier is not counted: no tilt, slow motion, or
// more balls than the options give.
TEST(cheated_games_are_not_counted) {
  if (!haveData()) return;
  const Bytes prg = read("TABLE2.PRG"), mod = read("TABLE2.MOD");
  Replay honest;
  play(1, 20000, 5, &honest);
  CHECK(honest.games.size() == 1);
  CHECK(verify(prg, mod, honest).ok);
  Replay snail = honest, earthquake = honest, extra = honest;
  snail.carry.slowdown = true;
  earthquake.carry.noTilt = true;
  extra.carry.balls = 5;
  for (const Replay* r : {&snail, &earthquake, &extra}) {
    const Verdict v = verify(prg, mod, *r);
    CHECK(!v.ok);
    CHECK(v.reason == "played with cheats");
  }
}

// A flipper's replacement picture turns about the point its own artwork hinges on, which is
// not always the origin the table gives for it: the upper bats of Party Land and Speed Devils
// draw the main bats' frames, hinged several pixels away from where their ball bounces off.
TEST(flipper_sprites_turn_about_the_artwork_hinge) {
  if (!haveData()) return;
  struct Expected { int table, flipper; float x, y; };
  // Fractions of the flipper's rectangle, measured from the tables' own flipper frames.
  for (const Expected& e : {Expected{0, 0, 15.14f / 64, 26.17f / 53}, Expected{0, 2, 6.23f / 48, 7.53f / 51},
                            Expected{1, 1, 43.86f / 64, 26.17f / 53}, Expected{1, 2, 43.60f / 64, 26.00f / 53},
                            Expected{3, 0, 15.14f / 64, 26.17f / 53}}) {
    Table t(read("TABLE" + std::to_string(e.table + 1) + ".PRG"), read("TABLE" + std::to_string(e.table + 1) + ".MOD"),
            Config::defaults(), e.table, 7);
    std::vector<u8> pixels(320 * (576 + 33));
    std::vector<Rgb> pal(256);
    HdFrame hd;
    t.render(pixels.data(), pal.data(), &hd);
    CHECK(hd.sprites.size() > static_cast<std::size_t>(e.flipper));
    if (hd.sprites.size() <= static_cast<std::size_t>(e.flipper)) continue;
    const HdSprite& s = hd.sprites[static_cast<std::size_t>(e.flipper)];
    std::printf("  table %d flipper %d turns about (%.3f, %.3f) of its rectangle\n", e.table + 1, e.flipper,
                s.pivotSpriteX, s.pivotSpriteY);
    CHECK(std::abs(s.pivotSpriteX - e.x) < 0.02f);
    CHECK(std::abs(s.pivotSpriteY - e.y) < 0.02f);
  }
}
