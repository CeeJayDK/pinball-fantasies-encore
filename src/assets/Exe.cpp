#include "assets/Exe.h"

namespace pfr {

Exe Exe::load(ByteView file, u16 ds) {
  if (file.size() < 0x1c || file[0] != 'M' || file[1] != 'Z') throw DataError("not an MZ executable");
  const std::size_t lastPage = rd16le(file, 2);
  const std::size_t pages = rd16le(file, 4);
  const std::size_t imageEnd = (pages - 1) * 0x200 + lastPage;
  const std::size_t header = static_cast<std::size_t>(rd16le(file, 8)) * 16;
  if (imageEnd > file.size() || header > imageEnd) throw DataError("truncated MZ executable");
  Exe e;
  e.image_.assign(file.begin() + static_cast<std::ptrdiff_t>(header), file.begin() + static_cast<std::ptrdiff_t>(imageEnd));
  e.ip = rd16le(file, 0x14);
  e.cs = rd16le(file, 0x16);
  e.ds = ds;
  return e;
}

}  // namespace pfr
