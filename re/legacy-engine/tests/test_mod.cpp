#include <cmath>

#include "Test.h"
#include "audio/ModPlayer.h"

using namespace pfr;

namespace {
ModFile makeModule() {
  Bytes b(1084 + 1024, 0);
  const char* title = "test";
  for (int i = 0; title[i]; ++i) b[i] = static_cast<u8>(title[i]);
  // sample 1: 64 bytes, volume 64, loop whole
  b[20 + 22] = 0; b[20 + 23] = 32;  // length words = 32 -> 64 bytes
  b[20 + 25] = 64;
  b[20 + 28] = 0; b[20 + 29] = 32;  // loop length
  b[950] = 1; b[951] = 0; b[952] = 0;
  b[1080] = 'M'; b[1081] = '.'; b[1082] = 'K'; b[1083] = '.';
  // row 0 channel 0: sample 1, period 428 (C-2)
  b[1084 + 0] = 0x01; b[1084 + 1] = 0xac; b[1084 + 2] = 0x10; b[1084 + 3] = 0;
  for (int i = 0; i < 64; ++i) b.push_back(static_cast<u8>(i < 32 ? 100 : -100));
  return ModFile::parse(b, "test");
}
}  // namespace

TEST(mod_parse_header) {
  ModFile m = makeModule();
  CHECK_EQ(m.title, std::string("test"));
  CHECK_EQ(m.samples[0].length, 64u);
  CHECK_EQ(m.patternCount, 1);
  CHECK_EQ(m.note(0, 0, 0).sample, 1);
  CHECK_EQ(m.note(0, 0, 0).period, 428);
}

TEST(mod_player_produces_audio) {
  ModFile m = makeModule();
  ModPlayer p(48000);
  p.setModule(&m);
  p.play();
  std::vector<float> out(4800 * 2);
  p.render(out.data(), 4800);
  double energy = 0;
  for (float v : out) energy += std::fabs(v);
  CHECK(energy > 1.0);
}

TEST(mod_player_sfx_override) {
  ModFile m = makeModule();
  ModPlayer p(48000);
  p.setModule(&m);
  p.setMusicEnabled(false);
  p.triggerSample(3, 1, 428, 64);
  std::vector<float> out(480 * 2);
  p.render(out.data(), 480);
  double energy = 0;
  for (float v : out) energy += std::fabs(v);
  CHECK(energy > 0.5);
}
