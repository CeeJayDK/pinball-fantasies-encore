#pragma once
// A score, held the way the original holds it: twelve decimal digits, one per byte.
// This is also the format the high-score files use, so scores move between them unchanged.
#include <array>
#include <string>

#include "core/Types.h"

namespace encore {

class Score {
 public:
  static constexpr int kDigits = 12;

  Score() = default;
  explicit Score(u64 value);
  /// Reads twelve digit bytes, as stored in the game's own tables.
  static Score fromDigits(ByteView data, std::size_t offset);

  void add(const Score& other);
  void addMultiplied(const Score& other, int times);
  void clear() { digits_.fill(0); }
  bool isZero() const;

  u64 value() const;
  const std::array<u8, kDigits>& digits() const { return digits_; }
  /// The score as the display shows it, without leading zeros.
  std::string text() const;

 private:
  std::array<u8, kDigits> digits_{};
};

}  // namespace encore
