#include "gfx/Palette.h"

#include <algorithm>

namespace pfr {

Palette::Palette() = default;

void Palette::set(std::size_t first, const std::vector<Rgb>& rgb) {
  for (std::size_t i = 0; i < rgb.size() && first + i < colors_.size(); ++i) colors_[first + i] = rgb[i];
}

void Palette::quantizeTo6Bit() {
  for (auto& c : colors_) {
    c.r = static_cast<u8>((c.r >> 2) * 255 / 63);
    c.g = static_cast<u8>((c.g >> 2) * 255 / 63);
    c.b = static_cast<u8>((c.b >> 2) * 255 / 63);
  }
}

void Palette::cycle(const std::vector<ColorRange>& ranges, double dtSeconds) {
  cycleAccumulators_.resize(ranges.size(), 0.0);
  for (std::size_t i = 0; i < ranges.size(); ++i) {
    const ColorRange& r = ranges[i];
    if (!(r.flags & 1) || r.rate == 0 || r.high <= r.low) continue;
    const double stepsPerSecond = r.rate * 60.0 / 16384.0;
    cycleAccumulators_[i] += dtSeconds * stepsPerSecond;
    while (cycleAccumulators_[i] >= 1.0) {
      cycleAccumulators_[i] -= 1.0;
      if (r.flags & 2) {
        std::rotate(colors_.begin() + r.low, colors_.begin() + r.low + 1, colors_.begin() + r.high + 1);
      } else {
        std::rotate(colors_.begin() + r.low, colors_.begin() + r.high, colors_.begin() + r.high + 1);
      }
    }
  }
}

}  // namespace pfr
