#include "game/Menu.h"

#include <algorithm>
#include <cstring>

#include "core/Log.h"

namespace pfr {

void Menu::init(const GameFiles& files, const IntroData& data) {
  data_ = &data;
  for (int i = 0; i < 4; ++i) hiScores_[i] = loadHiScores(files.hiScores[i]);
  // The first three bands are the backdrop and the two character strips the original keeps
  // off screen to copy text from. The menu proper begins at the first table panel.
  if (data_->menuBands.size() > 3) viewOrigin_ = data_->menuBands[3].row;
  scroll_ = targetScroll_ = viewOrigin_;
}

void Menu::update(const MenuInput& input, double dt) {
  const int maxScroll = std::max(viewOrigin_, data_->menuHeight - kHeight);
  if (input.escape) quit_ = true;
  if (input.functionKey >= 1 && input.functionKey <= 4) chosen_ = input.functionKey;

  // The original pages the screen between the upper and lower halves of the list.
  if (input.page) targetScroll_ = targetScroll_ > viewOrigin_ ? viewOrigin_ : maxScroll;
  if (input.scrollUp) targetScroll_ = std::max(viewOrigin_, targetScroll_ - 4);
  if (input.scrollDown) targetScroll_ = std::min(maxScroll, targetScroll_ + 4);
  targetScroll_ = std::clamp(targetScroll_, viewOrigin_, maxScroll);

  // Ease towards the target so paging glides rather than jumping.
  const int distance = targetScroll_ - scroll_;
  if (distance != 0) {
    const int step = std::max(1, static_cast<int>(std::abs(distance) * dt * 8.0));
    scroll_ += distance > 0 ? std::min(step, distance) : std::max(-step, distance);
  }
  pageTimer_ += dt;
}

void Menu::render(Framebuffer& frame, std::vector<Rgb>& rowPalettes) const {
  if (frame.width() != kWidth || frame.height() != kHeight) frame = Framebuffer(kWidth, kHeight);
  frame.clear(0);
  rowPalettes.assign(static_cast<std::size_t>(kHeight) * 256, Rgb{});
  for (int y = 0; y < kHeight; ++y) {
    const int source = scroll_ + y;
    if (source < 0 || source >= data_->menuHeight) continue;
    std::memcpy(frame.row(y), data_->menuPixels.data() + static_cast<std::size_t>(source) * kWidth, kWidth);
    const auto& palette = data_->menuRowPalettes[static_cast<std::size_t>(source)];
    std::copy(palette.begin(), palette.end(), rowPalettes.begin() + static_cast<std::size_t>(y) * 256);
  }
}

}  // namespace pfr
