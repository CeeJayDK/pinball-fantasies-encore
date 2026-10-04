#include "engine/game/Score.h"

namespace encore {

Score::Score(u64 value) {
  for (int i = kDigits - 1; i >= 0; --i) {
    digits_[static_cast<std::size_t>(i)] = static_cast<u8>(value % 10);
    value /= 10;
  }
}

Score Score::fromDigits(ByteView data, std::size_t offset) {
  Score s;
  if (offset + kDigits > data.size()) return s;
  for (int i = 0; i < kDigits; ++i) s.digits_[static_cast<std::size_t>(i)] = data[offset + i] % 10;
  return s;
}

void Score::add(const Score& other) {
  int carry = 0;
  for (int i = kDigits - 1; i >= 0; --i) {
    const int sum = digits_[static_cast<std::size_t>(i)] + other.digits_[static_cast<std::size_t>(i)] + carry;
    digits_[static_cast<std::size_t>(i)] = static_cast<u8>(sum % 10);
    carry = sum / 10;
  }
}

void Score::addMultiplied(const Score& other, int times) {
  for (int i = 0; i < times; ++i) add(other);
}

bool Score::isZero() const {
  for (u8 d : digits_)
    if (d != 0) return false;
  return true;
}

u64 Score::value() const {
  u64 v = 0;
  for (u8 d : digits_) v = v * 10 + d;
  return v;
}

std::string Score::text() const {
  std::string out;
  for (u8 d : digits_) {
    if (out.empty() && d == 0) continue;
    out.push_back(static_cast<char>('0' + d));
  }
  return out.empty() ? "0" : out;
}

}  // namespace encore
