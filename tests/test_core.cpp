// Pure-logic tests that need no game files.
#include "Test.h"

#include <optional>
#include "core/Bcd.h"
#include "core/Sha256.h"
#include "sound/Sequencer.h"

using namespace pfr;

TEST(bcd_arithmetic) {
  CHECK(Bcd::of("999") + Bcd::of("1") == Bcd::of("1000"));
  CHECK(Bcd::of("1250") * 4 == Bcd::of("5000"));
  CHECK(Bcd::of("123").leadingZeros() == 9);
  CHECK(Bcd::kZero.isZero());
  const auto a = Bcd::of("1030060").toAscii();
  CHECK(std::string(a.begin(), a.end()) == "     1030060");
  const auto z = Bcd::kZero.toAscii();
  CHECK(std::string(z.begin(), z.end()) == "           0");
  CHECK(Bcd::of("50000000") > Bcd::of("25000000"));
}

TEST(sha256_known_vectors) {
  const std::string abc = "abc";
  CHECK_EQ(sha256Hex(ByteView(reinterpret_cast<const u8*>(abc.data()), abc.size())),
           std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  CHECK_EQ(sha256Hex(ByteView()), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

// A jingle interrupts the music, repeats, and hands back to the music afterwards; a
// lower-priority jingle cannot interrupt a higher one unless forced.
TEST(table_sequencer_jingles) {
  TableSequencer seq(/*position*/ 0, /*jingleStart*/ 10, /*silence*/ 5, /*noMusic*/ false);
  CHECK(seq.checkInterrupt() == std::optional<u8>(0));
  CHECK(!seq.checkInterrupt());
  CHECK_EQ(seq.nextPosition(), 1);
  CHECK(seq.playJingle({20, 2, 5}, false, std::nullopt));
  CHECK(seq.jinglePlaying());
  CHECK(!seq.playJingle({30, 1, 3}, false, std::nullopt));  // lower priority refused
  CHECK(seq.checkInterrupt() == std::optional<u8>(20));
  CHECK_EQ(seq.jump(20), 20);  // first repeat
  CHECK_EQ(seq.jump(20), 1);   // repeats used up: back to the music
  CHECK(seq.jinglePlaying());  // the game sees what the player did at its next frame
  seq.sync();
  CHECK(!seq.jinglePlaying());
  CHECK_EQ(seq.priority(), 0);
}

TEST(table_sequencer_no_music) {
  TableSequencer seq(0, 10, 5, true);
  seq.checkInterrupt();
  CHECK_EQ(seq.jump(3), 5);    // music positions play silence
  CHECK_EQ(seq.jump(12), 12);  // jingles still play
}
