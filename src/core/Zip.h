#pragma once
// Reading a zip archive held in memory: the entries it lists, unpacked with this project's
// own inflate. Stored and deflated entries only, which is all a normal archive holds.
#include <string>
#include <vector>

#include "core/Types.h"

namespace pfr {

struct ZipEntry {
  std::string name;  ///< as the archive spells it, with forward slashes
  Bytes data;
};

/// Every file in the archive, folders left out. Empty when it cannot be read.
std::vector<ZipEntry> readZip(ByteView zip);

}  // namespace pfr
