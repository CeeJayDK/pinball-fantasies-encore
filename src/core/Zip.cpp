#include "core/Zip.h"

#include "core/Inflate.h"
#include "core/Log.h"

namespace encore {
namespace {

constexpr u32 kEndOfCentralDirectory = 0x06054b50;
constexpr u32 kCentralFile = 0x02014b50;
constexpr u32 kLocalFile = 0x04034b50;

/// The end record sits last, behind a comment of up to 64 KB.
std::size_t findEnd(ByteView zip) {
  if (zip.size() < 22) return std::string::npos;
  const std::size_t furthest = zip.size() > 22 + 0xffff ? zip.size() - 22 - 0xffff : 0;
  for (std::size_t i = zip.size() - 22 + 1; i-- > furthest;)
    if (rd32le(zip, i) == kEndOfCentralDirectory) return i;
  return std::string::npos;
}

}  // namespace

std::vector<ZipEntry> readZip(ByteView zip) {
  const std::size_t end = findEnd(zip);
  if (end == std::string::npos) {
    log::error("not a zip archive");
    return {};
  }
  const std::size_t count = rd16le(zip, end + 10);
  std::size_t at = rd32le(zip, end + 16);

  std::vector<ZipEntry> entries;
  entries.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    if (at + 46 > zip.size() || rd32le(zip, at) != kCentralFile) break;
    const u16 method = rd16le(zip, at + 10);
    const u32 packed = rd32le(zip, at + 20), unpacked = rd32le(zip, at + 24);
    const std::size_t nameLen = rd16le(zip, at + 28), extraLen = rd16le(zip, at + 30),
                      commentLen = rd16le(zip, at + 32), local = rd32le(zip, at + 42);
    if (at + 46 + nameLen > zip.size()) break;
    std::string name(reinterpret_cast<const char*>(zip.data() + at + 46), nameLen);
    at += 46 + nameLen + extraLen + commentLen;
    if (name.empty() || name.back() == '/') continue;  // a folder

    // The local header repeats the name, and its own extra field is usually a different size.
    if (local + 30 > zip.size() || rd32le(zip, local) != kLocalFile) continue;
    const std::size_t from = local + 30 + rd16le(zip, local + 26) + rd16le(zip, local + 28);
    if (from + packed > zip.size()) continue;
    const ByteView stored(zip.data() + from, packed);

    ZipEntry entry;
    entry.name = std::move(name);
    if (method == 0) {
      entry.data.assign(stored.begin(), stored.end());
    } else if (method == 8) {
      entry.data.reserve(unpacked);
      if (!inflateRaw(stored, entry.data)) {
        log::error(entry.name + ": cannot be unpacked");
        continue;
      }
    } else {
      log::error(entry.name + ": packed in a way this reader does not know");
      continue;
    }
    if (entry.data.size() != unpacked) {
      log::error(entry.name + ": unpacked to the wrong size");
      continue;
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

}  // namespace encore
