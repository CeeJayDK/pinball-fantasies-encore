#include "Test.h"
#include "engine/data/HiScoreFile.h"

using namespace encore;

TEST(hiscore_decode_original_bytes) {
  // Bytes exactly as written by the original game (TABLE1.HI with the default table).
  const Bytes raw = {0,0,0,0,0,5,0,0,0,0,0,0,'T','S','P',0,
                     0,0,0,0,0,2,5,0,0,0,0,0,'I','C','E',0,
                     0,0,0,0,0,1,0,0,0,0,0,0,'A','N','Y',0,
                     0,0,0,0,0,0,5,0,0,0,0,0,'J',' ','L',0};
  HiScoreTable t = HiScoreTable::decode(raw);
  CHECK_EQ(t.entries[0].score, 5000000u);
  CHECK_EQ(t.entries[0].name, std::string("TSP"));
  CHECK_EQ(t.entries[3].score, 500000u);
  CHECK_EQ(t.entries[3].name, std::string("J L"));
  CHECK(t.encode() == raw);
}

TEST(hiscore_insert) {
  HiScoreTable t = HiScoreTable::defaults();
  CHECK_EQ(t.rankFor(3'000'000), 1);
  CHECK_EQ(t.rankFor(1), -1);
  t.insert(1, {3'000'000, "PED"});
  CHECK_EQ(t.entries[1].name, std::string("PED"));
  CHECK_EQ(t.entries[2].name, std::string("ICE"));
  CHECK_EQ(t.entries[3].name, std::string("ANY"));
}
