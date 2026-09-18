#pragma once
// A 16-bit MZ executable as the game's code sees it: segment:offset access into the load
// image, with the entry code segment and the data segment the program sets up at start.
#include "assets/Bcd.h"
#include "core/Error.h"
#include "core/Types.h"

namespace pfr {

class Exe {
 public:
  Exe() = default;
  /// Parses the MZ header. `ds` is the data segment, or 0 to leave it unset.
  static Exe load(ByteView file, u16 ds = 0);

  u16 cs = 0, ip = 0, ds = 0;

  ByteView segment(u16 seg) const {
    const std::size_t off = static_cast<std::size_t>(seg) * 16;
    if (off > image_.size()) throw DataError("segment outside the executable");
    return ByteView(image_).subspan(off);
  }
  u8 byte(u16 seg, u16 off) const { return at(seg, off, 1)[0]; }
  u16 word(u16 seg, u16 off) const { return rd16le(at(seg, off, 2), 0); }
  i16 wordS(u16 seg, u16 off) const { return static_cast<i16>(word(seg, off)); }
  ByteView bytes(u16 seg, u16 off, std::size_t n) const { return at(seg, off, n); }

  u8 dataByte(u16 off) const { return byte(ds, off); }
  u16 dataWord(u16 off) const { return word(ds, off); }
  i16 dataWordS(u16 off) const { return wordS(ds, off); }
  ByteView dataBytes(u16 off, std::size_t n) const { return bytes(ds, off, n); }
  Bcd dataBcd(u16 off) const { return Bcd::fromBytes(dataBytes(off, Bcd::kDigits)); }

  u8 codeByte(u16 off) const { return byte(cs, off); }
  u16 codeWord(u16 off) const { return word(cs, off); }
  ByteView codeBytes(u16 off, std::size_t n) const { return bytes(cs, off, n); }

 private:
  ByteView at(u16 seg, u16 off, std::size_t n) const {
    const std::size_t p = static_cast<std::size_t>(seg) * 16 + off;
    if (p + n > image_.size()) throw DataError("read outside the executable");
    return ByteView(image_).subspan(p, n);
  }
  Bytes image_;
};

}  // namespace pfr
