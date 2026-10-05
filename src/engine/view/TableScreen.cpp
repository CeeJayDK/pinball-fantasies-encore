#include "engine/view/TableScreen.h"

#include <cmath>
#include <cstring>

namespace encore {

TableScreen::TableScreen(const std::filesystem::path& prg, int table) : data_(TableData::load(prg, table)) {
  ball_ = BallSprite::decode(data_.code);
}

TableScreen::TableScreen(Bytes prg, int table) : data_(TableData::parse(std::move(prg), "TABLE" + std::to_string(table + 1) + ".PRG", table)) {
  ball_ = BallSprite::decode(data_.code);
}

void TableScreen::attach(Engine& engine) {
  picture_ = data_.playfield;
  // (the picture's first row is rubbed out as the table starts: cs:31b5)
  std::fill_n(picture_.begin(), kWidth, u8{0});
  engine.onFlipperDrawn = [this, &engine](u16 record, u16 was, u16 now) { flipperDrawn(engine, record, was, now); };

  // Each flipper's shape at rest, from the first of the pictures of it that the ball is
  // stopped by: before the table's start-up adds the wall round it to them (cs:3d81).
  flipperRest_.clear();
  const u16 pictures[3] = {engine.S(0x4c54), engine.S(0x4fc2), engine.S(0x4eb6)};
  int which = 0;
  for (u16 f = engine.A(0x6950); which < 3 && engine.nativeB(f) != 0; f = static_cast<u16>(f + 0x3c), ++which) {
    const int bytes = engine.nativeW(static_cast<u16>(f + 0x06)) * 2, rows = engine.nativeW(static_cast<u16>(f + 0x08));
    Bytes shape(static_cast<std::size_t>(bytes * 8 * rows));
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < bytes * 8; ++x)
        shape[static_cast<std::size_t>(y * bytes * 8 + x)] =
            (engine.farB(pictures[which], static_cast<u16>(y * bytes + (x >> 3))) & (0x80 >> (x & 7))) ? 1 : 0;
    flipperRest_.push_back(std::move(shape));
  }
}

/// The original hides the ball dot by dot, and where something is to be seen through (the
/// criss-cross rail on Stones 'n Bones) hides every other dot. For a picture of the ball at
/// any size that is told as how much of the ball each dot hides, taken over the dots around it:
/// all of it under a ramp, half of it behind such a rail.
void TableScreen::started(Engine& e) {
  const u16 maps = e.S(0x2f94);
  for (std::size_t layer = 0; layer < 2; ++layer) {
    const u16 map = layer ? 0x5f80 : 0x0100;
    auto hidden = [&](int x, int y) {
      x = x < 0 ? 0 : x >= kWidth ? kWidth - 1 : x;
      y = y < 0 ? 0 : y >= TableData::kHeight ? TableData::kHeight - 1 : y;
      return (e.farB(maps, static_cast<u16>(map + y * 42 + (x >> 3))) & (0x80 >> (x & 7))) ? 1 : 0;
    };
    cover_[layer].assign(static_cast<std::size_t>(kWidth) * TableData::kHeight, 0);
    for (int y = 0; y < TableData::kHeight; ++y)
      for (int x = 0; x < kWidth; ++x) {
        int sum = 0;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) sum += hidden(x + dx, y + dy) * (dx ? 1 : 2) * (dy ? 1 : 2);
        cover_[layer][static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)] = static_cast<u8>(sum * 255 / 16);
      }
  }
}

/// cs:550e, cs:69aa: the original turns a flipper by copying, from one way it stands to the
/// next, only the groups of four dots that change: for every step there is a list of where in
/// the picture a group goes and where in the video card's memory it is kept, and another
/// for the step back. Turning several steps at once it leaves out of each list what a later
/// one will draw over, by a count kept for it, and not always all of that: so the same is
/// done here, list by list, on the picture kept for the screen.
void TableScreen::flipperDrawn(Engine& e, u16 record, u16 was, u16 now) {
  auto w = [&](u16 o) { return e.nativeW(static_cast<u16>(record + o)); };
  const u16 lists = e.S(0x0c0f);
  const auto& video = e.videoMemory();
  const std::size_t base = static_cast<std::size_t>(w(0x04)) * 0x54 + (w(0x02) >> 2);  // in groups of four dots, 84 to a row
  const u16 stride = w(0x38);
  u16 list = now > was ? static_cast<u16>(was * stride + w(0x36)) : static_cast<u16>(w(0x30) - was * stride);
  int steps = now > was ? now - was : was - now;
  while (steps > 0) {
    int left = steps > 9 ? 9 : steps;
    steps -= left;
    for (; left > 0; --left, list = static_cast<u16>(list + stride)) {
      const u16 count = e.farW(lists, static_cast<u16>(list + (left - 1) * 2));
      for (u16 i = 0; i < count; ++i) {
        const u16 entry = static_cast<u16>(list + 0x12 + i * 4);
        const std::size_t to = base + e.farW(lists, entry);
        const u16 from = e.farW(lists, static_cast<u16>(entry + 2));
        const std::size_t row = to / 0x54, column = to % 0x54;
        if (row >= TableData::kHeight || column >= 80) continue;
        for (std::size_t plane = 0; plane < 4; ++plane) picture_[row * kWidth + column * 4 + plane] = video[plane][from];
      }
    }
  }
}

/// How lit each colour of the picture is, 0 to 63: a light's colours are as the table gives
/// them when it is lit and half that when it is out (cs:5728, cs:5747), and the colours the
/// video card now has say which.
void TableScreen::lightAmounts(Engine& e, int lamps, std::array<u8, 256>& amount) const {
  amount.fill(0);
  const auto& dac = e.colours();
  for (u16 light = 1, lights = e.kw(0x5912, 1); light <= lights; ++light) {
    u16 record = e.W(0x12bd, static_cast<u16>((light - 1) * 2));
    u16 colour = e.nativeB(record++);
    const u8 bytes = e.nativeB(record++);
    for (u8 i = 0; i + 2 < bytes && colour < 256; i = static_cast<u8>(i + 3), ++colour) {
      int best = 0, lit = 0;
      for (u16 part = 0; part < 3; ++part) {
        const int full = e.nativeB(static_cast<u16>(record + i + part)) & 0x3f, half = full >> 1;
        if (full - half <= best) continue;
        best = full - half;
        lit = (dac[colour * 3u + part] - half) * 63 / (full - half);
      }
      amount[colour] = static_cast<u8>(lamps == 1 ? 63 : lamps == 2 ? 0 : lit < 0 ? 0 : lit > 63 ? 63 : lit);
    }
  }
}

void TableScreen::colours(Engine& e, Rgb* out, int lamps) const {
  std::array<u8, 768> dac = e.colours();
  if (lamps != 0)
    for (u16 light = 1, lights = e.kw(0x5912, 1); light <= lights; ++light) {
      u16 record = e.W(0x12bd, static_cast<u16>((light - 1) * 2));
      u16 at = static_cast<u16>(e.nativeB(record++) * 3);
      const u8 bytes = e.nativeB(record++);
      for (u8 i = 0; i < bytes && at < 768; ++i, ++at) {
        const u8 full = e.nativeB(record++) & 0x3f;
        dac[at] = lamps == 1 ? full : full >> 1;
      }
    }
  auto wide = [](u8 v) { return static_cast<u8>((v << 2) | (v >> 4)); };  // 0-63 as 0-255
  for (std::size_t i = 0; i < 256; ++i) out[i] = Rgb{wide(dac[i * 3]), wide(dac[i * 3 + 1]), wide(dac[i * 3 + 2])};
}

void TableScreen::draw(Engine& e, u8* frame, const View& v, HdFrame* hd) const {
  const int height = v.height, view = height - kDisplayRows, top = v.top;
  std::memset(frame, 0, static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(height));
  for (int y = 0; y < view; ++y) {
    const int row = top + y;
    if (row < 0 || row >= TableData::kHeight || picture_.empty()) continue;
    std::memcpy(frame + static_cast<std::size_t>(y) * kWidth, picture_.data() + static_cast<std::size_t>(row) * kWidth, kWidth);
  }
  const Engine::Shown& shown = e.shown();
  const std::size_t layer = shown.ramps ? 1 : 0;
  if (hd) {
    // Every dot of the window is a dot of the playfield's picture, lit as its colour is.
    hd->reset(kWidth, height);
    const auto off = static_cast<u16>(static_cast<int>(HdPicture::Playfield1Off) + data_.index);
    const auto on = static_cast<u16>(static_cast<int>(HdPicture::Playfield1On) + data_.index);
    hd->used |= (1u << off) | (1u << on);
    hd->size[off] = hd->size[on] = {static_cast<u16>(kWidth), static_cast<u16>(TableData::kHeight)};
    std::array<u8, 256> amount;
    lightAmounts(e, v.lamps, amount);
    for (int y = 0; y < view; ++y) {
      const int row = top + y;
      if (row < 0 || row >= TableData::kHeight) continue;
      HdPixel* out = hd->map.data() + static_cast<std::size_t>(y) * kWidth;
      const u8* in = data_.playfield.data() + static_cast<std::size_t>(row) * kWidth;
      const u8* hides = cover_[layer].empty() ? nullptr : cover_[layer].data() + static_cast<std::size_t>(row) * kWidth;
      for (int x = 0; x < kWidth; ++x) {
        out[x].x8 = static_cast<u16>(x * 8);
        out[x].y8 = static_cast<u16>(row * 8);
        out[x].picture = static_cast<u16>(off | (amount[in[x]] << HdFrame::kLitShift));
        const u8 c = hides ? hides[x] : 0;
        out[x].flags = static_cast<u16>((c ? HdPixel::kHidesBall : 0) | (c << HdPixel::kCoverShift));
      }
    }
  }
  auto put = [&](int x, int row, u8 colour) {
    const int y = row - top;
    if (x >= 0 && x < kWidth && y >= 0 && y < view) frame[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)] = colour;
  };
  // the plunger (cs:6600): its picture moves down as it is pulled, and what it leaves is dark
  {
    const int width = e.kw(0x65e9, 1), rows = e.kw(0x6660, 1);
    const int at = e.kw(0x662a, 1) - 0x0ad4;  // where in the video card's memory, past the display
    const int row0 = at / 0x54, x0 = (at % 0x54) * 4;
    const int pull = static_cast<i8>((e.B(0x23a5) >> 1) - 3);
    const int down = pull < 0 ? 0 : pull, skip = pull < 0 ? -pull : 0;
    const auto plunger = static_cast<u16>(HdPicture::Plunger);
    if (hd) {
      hd->used |= 1u << plunger;
      hd->size[plunger] = {static_cast<u16>(width), static_cast<u16>(data_.plungerImage.size() / static_cast<std::size_t>(width))};
    }
    auto mark = [&](int x, int row, int px, int py) {  // a dot of the plunger's own picture, or of none
      const int y = row - top;
      if (!hd || x < 0 || x >= kWidth || y < 0 || y >= view) return;
      HdPixel& m = hd->map[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)];
      m = px < 0 ? HdPixel{} : HdPixel{static_cast<u16>(px * 8), static_cast<u16>(py * 8), plunger, 0};
    };
    for (int y = 0; y < down; ++y)
      for (int x = 0; x < width; ++x) {
        put(x0 + x, row0 + y, 0);
        mark(x0 + x, row0 + y, -1, -1);
      }
    for (int y = 0; y < rows - down; ++y)
      for (int x = 0; x < width; ++x) {
        const std::size_t i = static_cast<std::size_t>((skip + y) * width + x);
        if (i >= data_.plungerImage.size()) continue;
        put(x0 + x, row0 + down + y, data_.plungerImage[i]);
        mark(x0 + x, row0 + down + y, x, skip + y);
      }
  }
  // the ball, but for the dots of it that something on the table passes over (cs:95b0)
  if (ball_.valid() && !hd) {
    const u16 maps = e.S(0x2f94);
    const u16 map = shown.ramps ? 0x5f80 : 0x0100;
    for (int y = 0; y < ball_.height; ++y)
      for (int x = 0; x < ball_.width; ++x) {
        if (!ball_.covers(x, y)) continue;
        const int px = shown.ballX + x, py = shown.ballY + y;
        // (a ball leaving by the bottom is still drawn in the few rows past the picture that a
        // shaken table shows)
        if (px < 0 || px >= kWidth || py < 0 || py >= TableData::kHeight + 4) continue;
        if (e.farB(maps, static_cast<u16>(map + py * 42 + (px >> 3))) & (0x80 >> (px & 7))) continue;
        put(px, py, ball_.at(x, y));
      }
  }
  if (hd) {
    // The flippers, each one picture turned about its hinge by as much as it has risen. The
    // original has a picture for every 55 of a flipper's travel, about 3.2 degrees apart.
    constexpr float kStep = 3.185f * 3.14159265f / 180.0f;
    u16 which = 0;
    for (u16 f = e.A(0x6950); which < 3 && e.nativeB(f) != 0; f = static_cast<u16>(f + 0x3c), ++which) {
      auto w = [&](u16 o) { return static_cast<float>(static_cast<i16>(e.nativeW(static_cast<u16>(f + o)))); };
      const float width = w(0x06) * 16, rows = w(0x08);
      HdSprite s;
      s.picture = which;
      s.pivotFrameX = w(0x12) + 0.5f;
      s.pivotFrameY = w(0x14) + 0.5f - static_cast<float>(top);
      s.pivotSpriteX = (w(0x12) + 0.5f - w(0x02)) / width;
      s.pivotSpriteY = (w(0x14) + 0.5f - w(0x04)) / rows;
      s.scaleX = 1.0f / width;
      s.scaleY = 1.0f / rows;
      const float risen = w(0x1c) / 55.0f;
      s.angle = (e.nativeB(f) == 2 ? -risen : risen) * kStep;
      s.clipTop = 0;
      s.clipBottom = static_cast<float>(view);
      hd->sprites.push_back(s);
    }
    // The ball, where it is to the 1024th of a dot, and behind it where it was at its last steps.
    if (ball_.valid() && e.B(at::ballHidden) != 0xff) {
      const auto& steps = e.steps();
      const float lift = static_cast<float>(e.W(at::nudgeLift).s());
      auto ball = [&](const Engine::Step& at, float opacity) {
        HdSprite s;
        s.picture = HdSprite::kBall;
        s.pivotFrameX = static_cast<float>(at.x) / 1024.0f + static_cast<float>(ball_.width) / 2;
        s.pivotFrameY = static_cast<float>(at.y) / 1024.0f + lift + static_cast<float>(ball_.height) / 2 - static_cast<float>(top);
        s.pivotSpriteX = s.pivotSpriteY = 0.5f;
        s.scaleX = 1.0f / static_cast<float>(ball_.width);
        s.scaleY = 1.0f / static_cast<float>(ball_.height);
        s.clipTop = 0;
        s.clipBottom = static_cast<float>(view);
        s.hiddenBy = HdPixel::kHidesBall;
        s.opacity = opacity;
        hd->sprites.push_back(s);
      };
      const Engine::Step now{static_cast<i32>(u32{e.W(at::ballXFixed)} | (u32{e.W(at::ballXFixed, 2)} << 16)),
                             static_cast<i32>(u32{e.W(at::ballYFixed)} | (u32{e.W(at::ballYFixed, 2)} << 16))};
      if (v.ballTrail) {
        constexpr int kGhosts = 6;
        for (int g = kGhosts; g >= 1; --g) {
          const Engine::Step& was = steps[Engine::kSteps - 1 - static_cast<std::size_t>(g)];
          // (a ball put somewhere else leaves no trail from where it was)
          if (std::abs(was.x - now.x) > 48 * 1024 || std::abs(was.y - now.y) > 48 * 1024) continue;
          ball(was, 0.22f * static_cast<float>(kGhosts + 1 - g) / kGhosts);
        }
      }
      ball(now, 1.0f);
    }
  }

  // the display, which the video card shows below the playfield though it comes first in its
  // memory: a dot is a byte of the first or the third plane, every other column
  const auto& video = e.videoMemory();
  for (int y = 0; y < kDisplayRows; ++y) {
    u8* out = frame + static_cast<std::size_t>(view + y) * kWidth;
    for (int x = 0; x < kWidth; ++x) out[x] = video[static_cast<std::size_t>(x & 3)][static_cast<std::size_t>(y * 0x54 + (x >> 2))];
  }
}

std::vector<bool> TableScreen::flipperIsLeft(Engine& e) const {
  std::vector<bool> left;
  for (u16 f = e.A(0x6950); left.size() < 3 && e.nativeB(f) != 0; f = static_cast<u16>(f + 0x3c)) left.push_back(e.nativeB(f) == 2);
  return left;
}

std::vector<Cutout> TableScreen::flipperPictures(Engine& e) const {
  std::vector<Cutout> out;
  std::array<Rgb, 256> colour;
  colours(e, colour.data());
  std::size_t which = 0;
  for (u16 f = e.A(0x6950); which < flipperRest_.size() && e.nativeB(f) != 0; f = static_cast<u16>(f + 0x3c), ++which) {
    Cutout c;
    c.width = e.nativeW(static_cast<u16>(f + 0x06)) * 16;
    c.height = e.nativeW(static_cast<u16>(f + 0x08));
    const int x0 = e.nativeW(static_cast<u16>(f + 0x02)), y0 = e.nativeW(static_cast<u16>(f + 0x04));
    c.rgba.assign(static_cast<std::size_t>(c.width * c.height * 4), 0);
    for (int y = 0; y < c.height; ++y)
      for (int x = 0; x < c.width; ++x) {
        const std::size_t i = static_cast<std::size_t>(y * c.width + x);
        if (!flipperRest_[which][i] || x0 + x >= kWidth || y0 + y >= TableData::kHeight) continue;
        const Rgb rgb = colour[data_.playfield[static_cast<std::size_t>(y0 + y) * kWidth + static_cast<std::size_t>(x0 + x)]];
        c.rgba[i * 4] = rgb.r, c.rgba[i * 4 + 1] = rgb.g, c.rgba[i * 4 + 2] = rgb.b, c.rgba[i * 4 + 3] = 0xff;
      }
    out.push_back(std::move(c));
  }
  return out;
}

Cutout TableScreen::ballPicture(Engine& e) const {
  Cutout c;
  if (!ball_.valid()) return c;
  std::array<Rgb, 256> colour;
  colours(e, colour.data());
  c.width = ball_.width;
  c.height = ball_.height;
  c.rgba.assign(static_cast<std::size_t>(c.width * c.height * 4), 0);
  for (int y = 0; y < c.height; ++y)
    for (int x = 0; x < c.width; ++x) {
      if (!ball_.covers(x, y)) continue;
      const std::size_t i = static_cast<std::size_t>(y * c.width + x);
      const Rgb rgb = colour[ball_.at(x, y)];
      c.rgba[i * 4] = rgb.r, c.rgba[i * 4 + 1] = rgb.g, c.rgba[i * 4 + 2] = rgb.b, c.rgba[i * 4 + 3] = 0xff;
    }
  return c;
}

}  // namespace encore
