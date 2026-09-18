#include "data/HiScoreFile.h"

#include "core/File.h"

namespace pfr {

HiScoreTable HiScoreTable::defaults() {
  HiScoreTable t;
  t.entries = {{{5'000'000, "TSP"}, {2'500'000, "ICE"}, {1'000'000, "ANY"}, {500'000, "J L"}}};
  return t;
}

HiScoreTable HiScoreTable::decode(ByteView b) {
  if (b.size() < 64) return defaults();
  HiScoreTable t;
  for (int i = 0; i < kEntries; ++i) {
    const std::size_t o = static_cast<std::size_t>(i) * 16;
    u64 score = 0;
    for (int d = 0; d < 12; ++d) score = score * 10 + (b[o + d] % 10);
    std::string name;
    for (int c = 0; c < 3; ++c) {
      const u8 ch = b[o + 12 + c];
      if (ch == 0) break;
      name.push_back(static_cast<char>(ch));
    }
    t.entries[i] = {score, name};
  }
  return t;
}

Bytes HiScoreTable::encode() const {
  Bytes out(64, 0);
  for (int i = 0; i < kEntries; ++i) {
    const std::size_t o = static_cast<std::size_t>(i) * 16;
    u64 s = entries[i].score;
    for (int d = 11; d >= 0; --d) {
      out[o + d] = static_cast<u8>(s % 10);
      s /= 10;
    }
    for (std::size_t c = 0; c < 3 && c < entries[i].name.size(); ++c) out[o + 12 + c] = static_cast<u8>(entries[i].name[c]);
  }
  return out;
}

int HiScoreTable::rankFor(u64 score) const {
  for (int i = 0; i < kEntries; ++i)
    if (score > entries[i].score) return i;
  return -1;
}

void HiScoreTable::insert(int rank, const HiScoreEntry& entry) {
  if (rank < 0 || rank >= kEntries) return;
  for (int i = kEntries - 1; i > rank; --i) entries[i] = entries[i - 1];
  entries[rank] = entry;
}

HiScoreTable loadHiScores(const std::filesystem::path& path) {
  auto bytes = file::readAll(path);
  if (!bytes) return HiScoreTable::defaults();
  return HiScoreTable::decode(*bytes);
}

bool saveHiScores(const std::filesystem::path& path, const HiScoreTable& table) {
  return file::writeAll(path, table.encode());
}

}  // namespace pfr
