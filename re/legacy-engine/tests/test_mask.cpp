#include "Test.h"
#include "sim/Mask.h"

using namespace pfr;

TEST(mask_bit_order) {
  const Bytes packed = {0x80, 0x01};  // 16 px: first and last set
  Mask m = Mask::fromPacked(packed, 16, 1, 2);
  CHECK(m.get(0, 0));
  CHECK(!m.get(1, 0));
  CHECK(m.get(15, 0));
  CHECK(!m.get(16, 0));
  m.set(1, 0, true);
  CHECK(m.get(1, 0));
  m.patch(1, 0, 1, 1, Bytes{0xff});
  CHECK(m.get(8, 0));
}
