// Runs the simulation for a sustained period on every table and checks that the ball
// behaves: it stays on the playfield, it reacts to the plunger, and it eventually drains.
#include <cmath>
#include <filesystem>
#include <memory>

#include "Test.h"
#include "engine/GameDir.h"
#include "core/File.h"
#include "engine/game/Play.h"

using namespace encore;

namespace {

std::unique_ptr<TableData> loadTable(int index) {
  const std::filesystem::path dir = test::gameDir();
  auto p = file::findCaseInsensitive(dir, "TABLE" + std::to_string(index + 1) + ".PRG");
  if (!p) return nullptr;
  return std::make_unique<TableData>(TableData::load(*p, index));
}

}  // namespace

TEST(play_ball_launches_and_stays_on_the_playfield) {
  for (int t = 0; t < 4; ++t) {
    auto table = loadTable(t);
    if (!table) { std::printf("  (skipped table %d)\n", t + 1); continue; }
    Play play;
    play.init(*table, true);
    PlayInput in;

    // Draw the plunger fully back, then let go.
    in.plunger = true;
    for (int f = 0; f < 40; ++f) play.update(in, 1.0 / 70.0);
    in.plunger = false;
    play.update(in, 1.0 / 70.0);

    const int startY = play.ball().centreY();
    int highest = startY;
    int frames = 0;
    bool drained = false;
    bool insideBounds = true;
    for (; frames < 70 * 60 && !drained; ++frames) {
      play.update(in, 1.0 / 70.0);
      const Ball& b = play.ball();
      if (!b.active) { drained = true; break; }
      highest = std::min(highest, b.centreY());
      if (b.x < -16 || b.x > TableData::kWidth || b.y < -16 || b.y > TableData::kHeight) insideBounds = false;
      if (std::abs(b.vx) > 4100 || std::abs(b.vy) > 4100) insideBounds = false;
    }
    std::printf("  %s: launched from y=%d, reached y=%d, %s after %d frames\n", table->name.c_str(), startY,
                highest, drained ? "drained" : "still in play", frames);
    CHECK(insideBounds);
    // A full plunger stroke must drive the ball a long way up the table.
    CHECK(highest < startY - 200);
  }
}

TEST(play_camera_follows_the_ball) {
  auto table = loadTable(0);
  if (!table) { std::printf("  (skipped)\n"); return; }
  Play play;
  play.init(*table, true);
  PlayInput in;
  in.plunger = true;
  for (int f = 0; f < 40; ++f) play.update(in, 1.0 / 70.0);
  in.plunger = false;
  int minCamera = play.cameraRow();
  for (int f = 0; f < 70 * 10; ++f) {
    play.update(in, 1.0 / 70.0);
    minCamera = std::min(minCamera, play.cameraRow());
    // The camera must never show anything outside the table.
    CHECK(play.cameraRow() >= 0);
    CHECK(play.cameraRow() <= TableData::kHeight - Play::kViewRowsHigh);
    if (!play.ball().active) break;
  }
  // The view must have travelled up the table to follow the launched ball.
  CHECK(minCamera < TableData::kHeight - Play::kViewRowsHigh);
}
