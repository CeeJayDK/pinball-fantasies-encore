#pragma once
// Scores as the game keeps them: twelve decimal digits, most significant first, one per byte.
#include <array>
#include <string>
#include <string_view>

#include "core/Types.h"

namespace pfr {

struct Bcd {
  static constexpr std::size_t kDigits = 12;
  std::array<u8, kDigits> digits{};

  static Bcd fromBytes(ByteView b) {
    Bcd r;
    for (std::size_t i = 0; i < kDigits; ++i) r.digits[i] = static_cast<u8>(b[i] % 10);
    return r;
  }
  static Bcd fromDigit(u8 d) {
    Bcd r;
    r.digits[kDigits - 1] = d;
    return r;
  }
  /// "12345" -> 000000012345.
  static constexpr Bcd of(std::string_view s) {
    Bcd r;
    for (std::size_t i = 0; i < s.size(); ++i) r.digits[kDigits - s.size() + i] = static_cast<u8>(s[i] - '0');
    return r;
  }
  static const Bcd kZero;

  /// Right-aligned, leading zeros as spaces, at least one digit.
  std::array<u8, kDigits> toAscii() const {
    std::array<u8, kDigits> r{};
    bool seen = false;
    for (std::size_t i = 0; i < kDigits; ++i) {
      seen |= digits[i] != 0;
      r[i] = seen ? static_cast<u8>('0' + digits[i]) : ' ';
    }
    if (r[kDigits - 1] == ' ') r[kDigits - 1] = '0';
    return r;
  }
  std::size_t leadingZeros() const {
    std::size_t n = 0;
    while (n < kDigits && digits[n] == 0) ++n;
    return n;
  }
  bool isZero() const { return leadingZeros() == kDigits; }

  Bcd operator+(const Bcd& o) const {
    Bcd r = *this;
    u8 carry = 0;
    for (std::size_t i = kDigits; i-- > 0;) {
      r.digits[i] = static_cast<u8>(r.digits[i] + o.digits[i] + carry);
      carry = r.digits[i] >= 10;
      if (carry) r.digits[i] = static_cast<u8>(r.digits[i] - 10);
    }
    return r;
  }
  Bcd& operator+=(const Bcd& o) { return *this = *this + o; }
  /// Multiplication by a single digit.
  Bcd operator*(u8 m) const {
    Bcd r;
    unsigned carry = 0;
    for (std::size_t i = kDigits; i-- > 0;) {
      carry += digits[i] * m;
      r.digits[i] = static_cast<u8>(carry % 10);
      carry /= 10;
    }
    return r;
  }
  auto operator<=>(const Bcd&) const = default;
};

inline const Bcd Bcd::kZero{};

}  // namespace pfr
