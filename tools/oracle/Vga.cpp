#include "Vga.h"

namespace oracle {

Vga::Vga() {
  for (auto& p : planes) p.assign(0x10000, 0);
  seq[2] = 0x0f;
  gc[8] = 0xff;
}

void Vga::setMode(u8 mode) {
  for (auto& p : planes) p.assign(0x10000, 0);
  for (u8& v : gc) v = 0;
  gc[8] = 0xff;
  seq[2] = 0x0f;
  seq[4] = mode == 0x13 ? 0x0e : 0x06;  // chain-4 in mode 13h
  gc[5] = mode == 0x13 ? 0x40 : 0;
  for (u8& v : crtc) v = 0;
  crtc[0x18] = 0xff;
  crtc[0x07] = 0x10;
  crtc[0x09] = 0x40;
}

u8 Vga::read(u32 offset) {
  offset &= 0xffff;
  if (seq[4] & 0x08) return planes[offset & 3][offset & 0xfffc];  // chained
  for (int p = 0; p < 4; ++p) latch_[p] = planes[p][offset];
  if (gc[5] & 0x08) {  // read mode 1: colour compare
    u8 result = 0xff;
    for (int p = 0; p < 4; ++p) {
      if (!(gc[7] & (1 << p))) continue;
      const u8 want = (gc[2] & (1 << p)) ? 0xff : 0x00;
      result &= static_cast<u8>(~(latch_[p] ^ want));
    }
    return result;
  }
  return latch_[gc[4] & 3];
}

void Vga::write(u32 offset, u8 value) {
  offset &= 0xffff;
  if (seq[4] & 0x08) {
    if (seq[2] & (1 << (offset & 3))) planes[offset & 3][offset & 0xfffc] = value;
    return;
  }
  const int mode = gc[5] & 3;
  const u8 mask = gc[8];
  const int rot = gc[3] & 7;
  const int func = (gc[3] >> 3) & 3;
  auto logical = [&](u8 v, u8 latch) -> u8 {
    switch (func) {
      case 1: return v & latch;
      case 2: return v | latch;
      case 3: return v ^ latch;
      default: return v;
    }
  };
  for (int p = 0; p < 4; ++p) {
    if (!(seq[2] & (1 << p))) continue;
    u8 out = 0;
    switch (mode) {
      case 0: {
        u8 v = static_cast<u8>((value >> rot) | (value << (8 - rot)));
        if (gc[1] & (1 << p)) v = (gc[0] & (1 << p)) ? 0xff : 0x00;
        v = logical(v, latch_[p]);
        out = static_cast<u8>((v & mask) | (latch_[p] & ~mask));
        break;
      }
      case 1: out = latch_[p]; break;
      case 2: {
        u8 v = (value & (1 << p)) ? 0xff : 0x00;
        v = logical(v, latch_[p]);
        out = static_cast<u8>((v & mask) | (latch_[p] & ~mask));
        break;
      }
      default: {
        const u8 rotated = static_cast<u8>((value >> rot) | (value << (8 - rot)));
        const u8 m = rotated & mask;
        const u8 v = (gc[0] & (1 << p)) ? 0xff : 0x00;
        out = static_cast<u8>((v & m) | (latch_[p] & ~m));
        break;
      }
    }
    planes[p][offset] = out;
  }
}

u8 Vga::portIn(u16 port) {
  switch (port) {
    case 0x3c5: return seq[seqIndex_ & 7];
    case 0x3cf: return gc[gcIndex_ & 15];
    case 0x3d5: return crtc[crtcIndex_ & 31];
    case 0x3c9: {
      const u8 v = dac[(dacRead_ * 3 + dacReadStep_) % 768];
      if (++dacReadStep_ == 3) { dacReadStep_ = 0; ++dacRead_; }
      return v;
    }
    case 0x3da: {
      attrFlip_ = false;
      // Programs wait on these two bits. Within a frame the answers go: retrace for a while
      // (bits 0 and 3), then picture lines, each one read as display then blank.
      const unsigned n = statusReads_++;
      if (n < 8) return 0x09;
      // After many reads with no new frame, another retrace comes, so that a program that
      // waits for one outside the frame's callbacks is never stuck.
      const unsigned m = (n - 8) % 1024;
      if (m >= 1000) return 0x09;
      return (m & 1) ? 0x01 : 0x00;
    }
    default: return 0xff;
  }
}

void Vga::portOut(u16 port, u8 value) {
  switch (port) {
    case 0x3c4: seqIndex_ = value; break;
    case 0x3c5: seq[seqIndex_ & 7] = value; break;
    case 0x3ce: gcIndex_ = value; break;
    case 0x3cf: gc[gcIndex_ & 15] = value; break;
    case 0x3d4: crtcIndex_ = value; break;
    case 0x3d5: crtc[crtcIndex_ & 31] = value; break;
    case 0x3c0:
      if (!attrFlip_) attrIndex_ = value;
      else attr[attrIndex_ & 31] = value;
      attrFlip_ = !attrFlip_;
      break;
    case 0x3c7: dacRead_ = value; dacReadStep_ = 0; break;
    case 0x3c8: dacWrite_ = value; dacWriteStep_ = 0; break;
    case 0x3c9:
      dac[(dacWrite_ * 3 + dacWriteStep_) % 768] = value & 0x3f;
      if (++dacWriteStep_ == 3) { dacWriteStep_ = 0; ++dacWrite_; }
      break;
    default: break;
  }
}

std::vector<u8> Vga::picture(int height) const {
  std::vector<u8> out(static_cast<std::size_t>(320 * height));
  const int split = lineCompare();
  const int scan = (crtc[0x09] & 0x1f) + 1;  // screen lines per picture row
  u32 start = startAddress();
  const u32 pitch = crtc[0x13] ? crtc[0x13] * 2u : 80u;  // bytes of each plane per row
  int row = 0;
  bool below = false;
  for (int y = 0; y < height; ++y, ++row) {
    if (!below && y * scan > split) {  // below the split, memory restarts at 0
      below = true;
      start = 0;
      row = 0;
    }
    const u32 base = start + static_cast<u32>(row) * pitch;
    for (int x = 0; x < 320; ++x) out[static_cast<std::size_t>(y * 320 + x)] = planes[x & 3][(base + x / 4) & 0xffff];
  }
  return out;
}

}  // namespace oracle
