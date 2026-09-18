// The tables name their sounds as positions and notes inside their own music module.
#include <filesystem>

#include "Test.h"
#include "core/File.h"
#include "data/GameFiles.h"
#include "data/ModFile.h"
#include "data/TableData.h"
#include "data/TableSound.h"

using namespace pfr;

TEST(sounds_are_valid_notes_of_the_tables_own_module) {
  const std::filesystem::path dir = std::filesystem::path(PFR_SOURCE_DIR) / "..";
  for (int i = 0; i < 4; ++i) {
    auto prg = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".PRG");
    auto mod = file::findCaseInsensitive(dir, "TABLE" + std::to_string(i + 1) + ".MOD");
    if (!prg || !mod) { std::printf("  (skipped table %d)\n", i + 1); continue; }
    TableData t = TableData::load(*prg, i);
    ModFile music = ModFile::load(*mod);
    TableSounds s = extractSounds(t.dataSegment, i);

    int named = 0;
    for (const Sfx* e : {&s.flipper, &s.drained, &s.issueBall, &s.spring, &s.rollInner, &s.tickBonus,
                         &s.gameStart, &s.raiseTargets}) {
      if (!e->valid()) continue;
      ++named;
      // Every effect must name a sample the module actually has, and a real note.
      CHECK(e->sample >= 1 && e->sample <= 31);
      CHECK(music.samples[e->sample - 1].length > 0);
      CHECK(e->note >= 1 && e->note <= 36);
      CHECK(e->channel <= 3);
    }
    int tunes = 0;
    for (const Jingle* j : {&s.plunger, &s.main, &s.attract, &s.tilt, &s.drainedJingle, &s.matchWin}) {
      if (!j->valid()) continue;
      ++tunes;
      CHECK(j->position < music.songLength);
    }
    std::printf("  %s: %d effects, %d music cues, all within the module\n", t.name.c_str(), named, tunes);
    CHECK(named >= 6);
    CHECK(tunes >= 4);
  }
}

TEST(party_land_flipper_uses_its_named_sample) {
  const std::filesystem::path dir = std::filesystem::path(PFR_SOURCE_DIR) / "..";
  auto prg = file::findCaseInsensitive(dir, "TABLE1.PRG");
  auto mod = file::findCaseInsensitive(dir, "TABLE1.MOD");
  if (!prg || !mod) { std::printf("  (skipped)\n"); return; }
  TableSounds s = extractSounds(TableData::load(*prg, 0).dataSegment, 0);
  ModFile music = ModFile::load(*mod);
  CHECK_EQ(s.flipper.sample, 25);
  CHECK_EQ(s.flipper.note, 22);
  if (s.flipper.sample >= 1 && s.flipper.sample <= 31)
    std::printf("  flipper sample %d is \"%s\"\n", s.flipper.sample, music.samples[s.flipper.sample - 1].name.c_str());
  // Sound effects play on the last channel, leaving the first three for the music.
  CHECK_EQ(s.flipper.channel, 3);
}
