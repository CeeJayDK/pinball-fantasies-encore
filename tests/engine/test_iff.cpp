#include "Test.h"
#include "data/IffImage.h"

using namespace encore;

TEST(iff_pbm_byterun1_roundtrip) {
  // Build a tiny PBM FORM: 4x2, palette of 2, compressed body.
  Bytes f;
  auto be32 = [&](u32 v) { f.insert(f.end(), {u8(v >> 24), u8(v >> 16), u8(v >> 8), u8(v)}); };
  auto tag = [&](const char* t) { f.insert(f.end(), t, t + 4); };
  tag("FORM"); be32(0); tag("PBM ");
  tag("BMHD"); be32(20);
  f.insert(f.end(), {0, 4, 0, 2, 0, 0, 0, 0, 8, 0, 1, 0, 0, 0, 1, 1, 0, 4, 0, 2});
  tag("CMAP"); be32(6); f.insert(f.end(), {0, 0, 0, 255, 255, 255});
  tag("BODY");
  // rows of 4 bytes: row0 = 0 1 0 1 (literal), row1 = 1 1 1 1 (run)
  const Bytes body = {3, 0, 1, 0, 1, 0xfd, 1};
  be32(static_cast<u32>(body.size())); f.insert(f.end(), body.begin(), body.end()); f.push_back(0);
  const u32 size = static_cast<u32>(f.size() - 8);
  f[4] = u8(size >> 24); f[5] = u8(size >> 16); f[6] = u8(size >> 8); f[7] = u8(size);
  auto img = decodeIff(f);
  CHECK(img.has_value());
  if (!img) return;
  CHECK_EQ(img->width, 4);
  CHECK_EQ(img->height, 2);
  CHECK_EQ(img->palette.size(), 2u);
  CHECK_EQ(img->at(1, 0), 1);
  CHECK_EQ(img->at(0, 0), 0);
  CHECK_EQ(img->at(3, 1), 1);
}
