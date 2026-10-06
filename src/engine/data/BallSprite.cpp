#include "engine/data/BallSprite.h"

#include <algorithm>
#include <map>

#include "core/Log.h"

namespace encore {
namespace {

// Each pixel is written by `mov byte [si+displacement], colour`, in one of two encodings:
// C6 44 dd cc with a byte displacement, or C6 84 lo hi cc with a word one. The routine is
// the longest run of such writes in the whole code segment.
constexpr int kMaxGapWithinRoutine = 70;
// The writes are grouped into four sections, one per video plane, separated by the code
// that selects the next plane. Those breaks are far wider than the gaps inside a section.
constexpr int kSectionBreak = 50;
// The video memory layout the displacements are expressed in.
constexpr int kVramPitch = 84;
constexpr int kPlanes = 4;

}  // namespace

BallSprite BallSprite::decode(ByteView code) {
  std::vector<std::size_t> writes;
  for (std::size_t i = 0; i + 6 < code.size(); ++i)
    if (code[i] == 0xC6 && (code[i + 1] == 0x44 || code[i + 1] == 0x84)) writes.push_back(i);
  if (writes.empty()) return {};

  std::size_t bestStart = 0, bestCount = 0, start = 0;
  for (std::size_t i = 1; i <= writes.size(); ++i) {
    const bool broken = i == writes.size() || writes[i] - writes[i - 1] > kMaxGapWithinRoutine;
    if (!broken) continue;
    if (i - start > bestCount) { bestCount = i - start; bestStart = start; }
    start = i;
  }
  if (bestCount < 64) return {};

  std::map<std::pair<int, int>, u8> found;
  int plane = 0;
  for (std::size_t i = 0; i < bestCount; ++i) {
    const std::size_t at = writes[bestStart + i];
    if (i > 0 && writes[bestStart + i] - writes[bestStart + i - 1] >= kSectionBreak) ++plane;
    if (plane >= kPlanes) break;
    int displacement = 0;
    u8 colour = 0;
    if (code[at + 1] == 0x44) {
      displacement = code[at + 2];
      colour = code[at + 3];
    } else {
      displacement = rd16le(code, at + 2);
      colour = code[at + 4];
    }
    const int x = (displacement % kVramPitch) * kPlanes + plane;
    const int y = displacement / kVramPitch;
    found[{x, y}] = colour;
  }
  if (found.empty()) return {};

  BallSprite s;
  int maxX = 0, maxY = 0;
  for (const auto& [position, _] : found) {
    maxX = std::max(maxX, position.first);
    maxY = std::max(maxY, position.second);
  }
  s.width = maxX + 1;
  s.height = maxY + 1;
  s.pixels.assign(static_cast<std::size_t>(s.width) * s.height, 0);
  s.opaque.assign(static_cast<std::size_t>(s.width) * s.height, 0);
  for (const auto& [position, colour] : found) {
    const std::size_t index = static_cast<std::size_t>(position.second) * s.width + position.first;
    s.pixels[index] = colour;
    s.opaque[index] = 1;
  }
  return s;
}

}  // namespace encore
