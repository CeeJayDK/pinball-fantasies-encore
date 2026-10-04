#include "engine/game/Intro.h"

#include <algorithm>
#include <cstring>

#include "core/Log.h"

namespace encore {
namespace {

// The original held each logo until its music reached a given tick, at fifty ticks a
// second: 302, 620 and 891, then ran the title logo out to tick 1500.
constexpr double kFirstLogoEnds = 302.0 / 50.0;
constexpr double kSecondLogoEnds = 620.0 / 50.0;
constexpr double kThirdLogoEnds = 891.0 / 50.0;
constexpr double kPresentsEnds = 1030.0 / 50.0;
constexpr double kTitleEnds = 1500.0 / 50.0;

constexpr int kLogoWidth = IntroData::kIntroWidth;
constexpr int kLogoHeight = 240;
constexpr int kTitleWidth = 640;
constexpr int kTitleHeight = 480;

}  // namespace

void Intro::init(const IntroData& data) {
  data_ = &data;
  stages_.clear();
  current_ = 0;
  elapsed_ = 0;
  finished_ = false;

  const double ends[3] = {kFirstLogoEnds, kSecondLogoEnds, kThirdLogoEnds};
  for (std::size_t i = 0; i < data.introScreens.size() && i < 3; ++i)
    stages_.push_back({static_cast<int>(i), -1, ends[i], 0.33, 0.33});
  if (data.presentsPicture >= 0) stages_.push_back({-1, data.presentsPicture, kPresentsEnds, 0.13, 0.33});
  if (data.titlePicture >= 0) stages_.push_back({-1, data.titlePicture, kTitleEnds, 0.33, 0.5});
  if (stages_.empty()) finished_ = true;
}

void Intro::update(bool skip, double dt) {
  if (finished_) return;
  if (skip) {
    finished_ = true;
    return;
  }
  elapsed_ += dt;
  while (current_ < static_cast<int>(stages_.size()) && elapsed_ >= stage().end) ++current_;
  if (current_ >= static_cast<int>(stages_.size())) finished_ = true;
}

double Intro::brightness() const {
  if (finished_ || stages_.empty()) return 0;
  const Stage& s = stage();
  const double start = current_ == 0 ? 0.0 : stages_[static_cast<std::size_t>(current_ - 1)].end;
  const double into = elapsed_ - start;
  const double left = s.end - elapsed_;
  double level = 1.0;
  if (s.fadeIn > 0 && into < s.fadeIn) level = std::min(level, into / s.fadeIn);
  if (s.fadeOut > 0 && left < s.fadeOut) level = std::min(level, std::max(0.0, left / s.fadeOut));
  return std::clamp(level, 0.0, 1.0);
}

int Intro::width() const {
  if (finished_ || stages_.empty()) return kLogoWidth;
  return stage().picture == data_->titlePicture && stage().picture >= 0 ? kTitleWidth : kLogoWidth;
}

int Intro::height() const {
  if (finished_ || stages_.empty()) return kLogoHeight;
  return stage().picture == data_->titlePicture && stage().picture >= 0 ? kTitleHeight : kLogoHeight;
}

void Intro::render(Framebuffer& frame, Palette& palette) const {
  const int w = width();
  const int h = height();
  if (frame.width() != w || frame.height() != h) frame = Framebuffer(w, h);
  frame.clear(0);
  if (finished_ || stages_.empty() || !data_) return;

  const Stage& s = stage();
  std::array<Rgb, 256> colours{};
  if (s.screen >= 0 && s.screen < static_cast<int>(data_->introScreens.size())) {
    const IntroData::IntroScreen& screen = data_->introScreens[static_cast<std::size_t>(s.screen)];
    colours = screen.palette;
    for (int y = 0; y < h && y < screen.height; ++y) {
      const int source = screen.row + y;
      if (source < 0 || source >= data_->introHeight) continue;
      std::memcpy(frame.row(y), data_->introPixels.data() + static_cast<std::size_t>(source) * kLogoWidth,
                  static_cast<std::size_t>(std::min(w, kLogoWidth)));
    }
  } else if (s.picture >= 0 && s.picture < static_cast<int>(data_->pictures.size())) {
    const IffImage& picture = data_->pictures[static_cast<std::size_t>(s.picture)];
    for (std::size_t c = 0; c < 256; ++c) colours[c] = c < picture.palette.size() ? picture.palette[c] : Rgb{};
    // Centre the picture in the frame, as the original did on its taller screen.
    const int ox = std::max(0, (w - picture.width) / 2);
    const int oy = std::max(0, (h - picture.height) / 2);
    for (int y = 0; y < picture.height; ++y)
      for (int x = 0; x < picture.width; ++x) frame.put(ox + x, oy + y, picture.at(x, y));
  }

  const double level = brightness();
  for (std::size_t c = 0; c < 256; ++c) {
    palette[c] = {static_cast<u8>(colours[c].r * level), static_cast<u8>(colours[c].g * level),
                  static_cast<u8>(colours[c].b * level)};
  }
}

}  // namespace encore
