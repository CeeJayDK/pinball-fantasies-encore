// Verifies that the simulation decodes the same constants that were traced out of the
// original binary, and that a few elementary behaviours come out right.
#include <cmath>
#include <filesystem>

#include "Test.h"
#include "engine/GameDir.h"
#include "core/File.h"
#include "engine/sim/Physics.h"

using namespace encore;

namespace {

const TableData* partyLand() {
  static std::unique_ptr<TableData> cached;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const std::filesystem::path dir = test::gameDir();
    if (auto p = file::findCaseInsensitive(dir, "TABLE1.PRG")) cached = std::make_unique<TableData>(TableData::load(*p, 0));
  }
  return cached.get();
}

}  // namespace

TEST(physics_constants_match_the_binary) {
  const TableData* t = partyLand();
  if (!t) { std::printf("  (skipped: TABLE1.PRG not found)\n"); return; }
  Physics p;
  p.init(*t, true);
  const auto& l = p.limits();
  CHECK_EQ(l.velocityMin, -4100);
  CHECK_EQ(l.velocityMax, 4100);
  CHECK_EQ(l.bumperKick, -7000);
  CHECK_EQ(l.slingshotKick, -2000);
  CHECK_EQ(l.slingshotMinimum, -300);
  CHECK_EQ(l.nudgeLift, 600);
  CHECK_EQ(l.nudgeReturn, -200);
}

TEST(physics_bumpers_match_the_binary) {
  const TableData* t = partyLand();
  if (!t) { std::printf("  (skipped)\n"); return; }
  // Party Land has three pop bumpers and two slingshot kickers.
  auto all = extractBumpers(t->dataSegment, 0);
  int bumpers = 0, kickers = 0;
  for (const Bumper& b : all) (b.kicker ? kickers : bumpers)++;
  std::printf("  Party Land: %d bumpers, %d kickers\n", bumpers, kickers);
  CHECK_EQ(bumpers, 3);
  CHECK_EQ(kickers, 2);
  if (all.size() >= 3) {
    CHECK_EQ(all[0].rect.x, 211);
    CHECK_EQ(all[0].rect.y, 252);
    CHECK(!all[0].score.isZero());
    CHECK(all[0].sfx.valid());
  }
}

TEST(physics_flipper_records_match_the_binary) {
  const TableData* t = partyLand();
  if (!t) { std::printf("  (skipped)\n"); return; }
  Physics p;
  p.init(*t, true);
  const auto& f = p.flippers();
  CHECK_EQ(f[0].keySide, 2);           // left flipper, left key
  CHECK_EQ(f[0].pivotX, 95);
  CHECK_EQ(f[0].pivotY, 536);
  CHECK_EQ(f[0].maxFrame, 20);
  CHECK_EQ(f[0].frameWidth, 64);
  CHECK_EQ(f[0].frameRows, 53);
  CHECK_EQ(f[1].keySide, 1);           // right flipper, right key
  CHECK_EQ(f[1].pivotX, 204);
  CHECK_EQ(f[2].keySide, 2);           // upper flipper, also the left key
  CHECK_EQ(f[2].pivotX, 27);
  CHECK_EQ(f[2].maxFrame, 13);
  CHECK_EQ(f[2].frameWidth, 48);
  // Each record must resolve to its own frame bitmap.
  CHECK(f[0].frameStack >= 0);
  CHECK(f[1].frameStack >= 0);
  CHECK(f[2].frameStack >= 0);
  CHECK(f[0].frameStack != f[1].frameStack);
  CHECK(f[1].frameStack != f[2].frameStack);
}

TEST(physics_flipper_rises_in_eleven_substeps) {
  const TableData* t = partyLand();
  if (!t) { std::printf("  (skipped)\n"); return; }
  Physics p;
  p.init(*t, true);
  Flipper f = p.flippers()[0];
  int steps = 0;
  while (f.frame < f.maxFrame && steps < 100) { f.update(true, true); ++steps; }
  CHECK_EQ(steps, 11);
  CHECK_EQ(f.frame, f.maxFrame);
  // Releasing brings it back down, and slower than it went up.
  int fall = 0;
  while (f.frame > 0 && fall < 100) { f.update(false, true); ++fall; }
  CHECK_EQ(fall, 23);
}

TEST(physics_free_fall_accumulates_gravity_each_substep) {
  const TableData* t = partyLand();
  if (!t) { std::printf("  (skipped)\n"); return; }
  Physics p;
  p.init(*t, true);
  // Find a spot on the playfield where nothing touches the ball.
  Ball b;
  bool found = false;
  for (int y = 40; y < 520 && !found; y += 4)
    for (int x = 16; x < 300 && !found; x += 4) {
      b.place(x, y);
      if (!p.probe(b).hit) found = true;
    }
  CHECK(found);
  if (!found) return;
  const i32 startFixed = b.yFixed;
  b.active = true;
  Physics::Controls c;
  p.beginFrame(b);
  p.subStep(b, c);
  const int gravityPerStep = b.vy;   // the zone's downward pull, one sub-step's worth
  CHECK(gravityPerStep >= 8 && gravityPerStep <= 16);
  for (int i = 1; i < 10; ++i) p.subStep(b, c);
  CHECK_EQ(b.vy, gravityPerStep * 10);
  // Position is advanced before gravity is added, so ten sub-steps move the ball
  // 0+g+2g+...+9g = 45g sub-pixel units, well under a whole pixel.
  CHECK_EQ(b.yFixed - startFixed, 45 * gravityPerStep);
}

TEST(physics_ball_bounces_off_the_playfield_walls) {
  const TableData* t = partyLand();
  if (!t) { std::printf("  (skipped)\n"); return; }
  Physics p;
  p.init(*t, true);
  Ball b;
  b.place(160, 60);
  b.active = true;
  b.setVelocity(0, 2000);    // fire it downwards hard
  Physics::Controls c;
  bool bounced = false;
  for (int frame = 0; frame < 400 && !bounced && !b.lost; ++frame) {
    p.beginFrame(b);
    for (int s = 0; s < p.subStepsPerFrame(); ++s) {
      const i16 before = b.vy;
      p.subStep(b, c);
      if (before > 0 && b.vy < 0) bounced = true;   // reversed direction: a real bounce
    }
  }
  CHECK(bounced);
}

TEST(physics_initialises_for_every_table) {
  const std::filesystem::path dir = test::gameDir();
  for (int i = 0; i < 4; ++i) {
    auto path = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".PRG");
    if (!path) { std::printf("  (skipped table %d)\n", i + 1); continue; }
    TableData t = TableData::load(*path, i);
    Physics p;
    p.init(t, true);
    // Every table has a left and a right flipper, both with frames resolved.
    int resolved = 0;
    for (const Flipper& f : p.flippers())
      if (f.valid() && f.frameStack >= 0) ++resolved;
    std::printf("  %s: %d flippers resolved\n", t.name.c_str(), resolved);
    CHECK(resolved >= 2);
    CHECK_EQ(p.limits().velocityMax, 4100);
  }
}

TEST(occlusion_maps_hide_the_ball_only_under_features) {
  const std::filesystem::path dir = test::gameDir();
  for (int i = 0; i < 4; ++i) {
    auto path = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".PRG");
    if (!path) { std::printf("  (skipped table %d)\n", i + 1); continue; }
    TableData t = TableData::load(*path, i);
    CHECK_EQ(t.occlusionGround.width(), TableData::kWidth);
    CHECK_EQ(t.occlusionGround.height(), TableData::kHeight);
    int ground = 0, overhead = 0;
    for (int y = 0; y < TableData::kHeight; ++y)
      for (int x = 0; x < TableData::kWidth; ++x) {
        if (t.occlusionGround.get(x, y)) ++ground;
        if (t.occlusionOverhead.get(x, y)) ++overhead;
      }
    std::printf("  %s: %d ground, %d overhead occluded pixels\n", t.name.c_str(), ground, overhead);
    // Some of the table is covered, but nothing like all of it.
    CHECK(ground > 1000);
    CHECK(ground < TableData::kWidth * TableData::kHeight / 2);
    // The shooter lane and the space between the flippers must never hide the ball.
    CHECK(!t.occlusionGround.get(308, 544));
    CHECK(!t.occlusionGround.get(160, 545));
  }
}

TEST(bumpers_extract_from_every_table) {
  const std::filesystem::path dir = test::gameDir();
  for (int i = 0; i < 4; ++i) {
    auto path = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".PRG");
    if (!path) continue;
    TableData t = TableData::load(*path, i);
    auto all = extractBumpers(t.dataSegment, i);
    int bumpers = 0, kickers = 0;
    u64 highest = 0;
    for (const Bumper& b : all) {
      (b.kicker ? kickers : bumpers)++;
      highest = std::max(highest, b.score.value());
      CHECK(b.sfx.valid());
    }
    std::printf("  %s: %d bumpers, %d kickers, best score %llu\n", t.name.c_str(), bumpers, kickers,
                static_cast<unsigned long long>(highest));
    CHECK(!all.empty());
    CHECK(highest > 0);
  }
}
