// The ball's image is recovered from the instructions of the original's draw routine.
#include <filesystem>
#include <set>

#include "Test.h"
#include "engine/GameDir.h"
#include "core/File.h"
#include "engine/data/BallSprite.h"
#include "engine/data/TableData.h"

using namespace encore;

TEST(ball_sprite_decodes_from_every_table) {
  const std::filesystem::path dir = test::gameDir();
  std::set<std::pair<int, int>> shapeOfFirst;
  for (int i = 0; i < 4; ++i) {
    auto path = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".PRG");
    if (!path) { std::printf("  (skipped table %d)\n", i + 1); continue; }
    TableData table = TableData::load(*path, i);
    BallSprite ball = BallSprite::decode(table.code);
    CHECK(ball.valid());
    if (!ball.valid()) continue;
    CHECK_EQ(ball.width, 15);
    CHECK_EQ(ball.height, 15);
    std::set<std::pair<int, int>> shape;
    for (int y = 0; y < ball.height; ++y)
      for (int x = 0; x < ball.width; ++x)
        if (ball.covers(x, y)) shape.insert({x, y});
    std::printf("  %s: %zu pixels\n", table.name.c_str(), shape.size());
    // A 15x15 disc: 177 pixels, and the corners must be empty.
    CHECK_EQ(shape.size(), 177u);
    CHECK(!ball.covers(0, 0));
    CHECK(!ball.covers(14, 14));
    CHECK(ball.covers(7, 0));
    CHECK(ball.covers(0, 7));
    // Every table draws the same silhouette, only the shading colours differ.
    if (shapeOfFirst.empty()) shapeOfFirst = shape;
    else CHECK(shape == shapeOfFirst);
  }
}
