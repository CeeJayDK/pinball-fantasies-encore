#pragma once
// The video card as the game's programs use it: four planes of 64 KB, written through the
// sequencer's and graphics controller's registers, and read back, which the tables do to
// test the ball against masks they keep off screen.
#include <array>
#include <vector>

#include "Cpu.h"

namespace oracle {

class Vga {
 public:
  Vga();
  u8 read(u32 offset);              ///< offset into the 64 KB window at A000:0
  void write(u32 offset, u8 value);
  u8 portIn(u16 port);
  void portOut(u16 port, u8 value);
  /// A mode set through the BIOS: 0x13 puts the card in its chained 256-colour state.
  void setMode(u8 mode);
  /// Called once per frame: the retrace begins.
  void startFrame() { statusReads_ = 0; }

  std::array<std::vector<u8>, 4> planes;
  std::array<u8, 768> dac{};  ///< 6-bit components
  u8 seq[8]{}, gc[16]{}, crtc[32]{}, attr[32]{};

  u16 startAddress() const { return static_cast<u16>((crtc[0x0c] << 8) | crtc[0x0d]); }
  int lineCompare() const { return crtc[0x18] | ((crtc[0x07] & 0x10) << 4) | ((crtc[0x09] & 0x40) << 3); }
  /// The picture as shown: `height` rows of 320 colour indices, read as the unchained
  /// 256-colour mode lays them out, with the split screen the tables use for the display.
  std::vector<u8> picture(int height) const;

 private:
  u8 latch_[4]{};
  u8 seqIndex_ = 0, gcIndex_ = 0, crtcIndex_ = 0, attrIndex_ = 0;
  bool attrFlip_ = false;
  u8 dacWrite_ = 0, dacRead_ = 0;
  int dacWriteStep_ = 0, dacReadStep_ = 0;
  unsigned statusReads_ = 0;
};

}  // namespace oracle
