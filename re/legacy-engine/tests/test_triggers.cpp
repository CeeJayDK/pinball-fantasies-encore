// The triggers are the rectangles that tell the game what the ball did.
#include <filesystem>

#include "Test.h"
#include "core/File.h"
#include "data/TableData.h"
#include "data/Triggers.h"

using namespace pfr;

TEST(triggers_extract_from_every_table) {
  const std::filesystem::path dir = std::filesystem::path(PFR_SOURCE_DIR) / "..";
  for (int i = 0; i < 4; ++i) {
    auto path = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".PRG");
    if (!path) { std::printf("  (skipped table %d)\n", i + 1); continue; }
    TableData t = TableData::load(*path, i);
    TableTriggers tr = extractTriggers(t.dataSegment, i);
    std::printf("  %s: %zu hit, %zu roll on the playfield, %zu on the ramps\n", t.name.c_str(), tr.hit.size(),
                tr.rollGround.size(), tr.rollOverhead.size());
    CHECK(!tr.rollGround.empty());
    CHECK(!tr.rollOverhead.empty());
    // Every rectangle must lie on the table.
    for (const auto* list : {&tr.hit, &tr.rollGround, &tr.rollOverhead})
      for (const TriggerArea& a : *list) {
        // A few rectangles use the hidden margin beyond the visible 320 pixels.
        CHECK(a.rect.x >= 0 && a.rect.x + a.rect.w <= 337);
        CHECK(a.rect.y >= 0 && a.rect.y + a.rect.h <= 601);
        CHECK(a.handler != 0);
      }
  }
}

TEST(party_land_triggers_are_the_expected_features) {
  const std::filesystem::path dir = std::filesystem::path(PFR_SOURCE_DIR) / "..";
  auto path = file::findCaseInsensitive(dir, "TABLE1.PRG");
  if (!path) { std::printf("  (skipped)\n"); return; }
  TableData t = TableData::load(*path, 0);
  TableTriggers tr = extractTriggers(t.dataSegment, 0);
  // Party Land's four hit triggers: the arcade button and the three ducks.
  CHECK_EQ(tr.hit.size(), 4u);
  if (tr.hit.size() == 4) {
    CHECK_EQ(tr.hit[0].handler, 0x134c);
    CHECK_EQ(tr.hit[1].handler, 0x13ae);
    CHECK_EQ(tr.hit[2].handler, 0x142e);
    CHECK_EQ(tr.hit[3].handler, 0x14ae);
    CHECK_EQ(tr.hit[0].rect.x, 130);
    CHECK_EQ(tr.hit[0].rect.y, 196);
  }
  // The orbits and the tunnel are roll triggers on the playfield.
  bool orbitLeft = false, orbitRight = false, tunnel = false;
  for (const TriggerArea& a : tr.rollGround) {
    if (a.handler == 0x16b6) orbitLeft = true;
    if (a.handler == 0x18af) orbitRight = true;
    if (a.handler == 0x1b29) tunnel = true;
  }
  CHECK(orbitLeft);
  CHECK(orbitRight);
  CHECK(tunnel);
}
