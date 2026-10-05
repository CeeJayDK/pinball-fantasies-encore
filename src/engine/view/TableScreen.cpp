#include "engine/view/TableScreen.h"

#include <cstring>

namespace encore {

TableScreen::TableScreen(const std::filesystem::path& prg, int table) : data_(TableData::load(prg, table)) {
  ball_ = BallSprite::decode(data_.code);
}

void TableScreen::attach(Engine& engine) {
  picture_ = data_.playfield;
  // (the picture's first row is rubbed out as the table starts: cs:31b5)
  std::fill_n(picture_.begin(), kWidth, u8{0});
  engine.onFlipperDrawn = [this, &engine](u16 record, u16 was, u16 now) { flipperDrawn(engine, record, was, now); };
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

void TableScreen::draw(Engine& e, u8* frame, int height) const {
  const int view = height - kDisplayRows;
  std::memset(frame, 0, static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(height));
  // the window on the playfield: the row at its top is the engine's, less the display's rows
  // that come before the picture in the video card's memory
  const int top = static_cast<i16>(e.screenRow()) - kDisplayRows;
  for (int y = 0; y < view; ++y) {
    const int row = top + y;
    if (row < 0 || row >= TableData::kHeight || picture_.empty()) continue;
    std::memcpy(frame + static_cast<std::size_t>(y) * kWidth, picture_.data() + static_cast<std::size_t>(row) * kWidth, kWidth);
  }
  auto put = [&](int x, int row, u8 colour) {
    const int y = row - top;
    if (x >= 0 && x < kWidth && y >= 0 && y < view) frame[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)] = colour;
  };
  const Engine::Shown& shown = e.shown();
  // the plunger (cs:6600): its picture moves down as it is pulled, and what it leaves is dark
  {
    const int width = e.kw(0x65e9, 1), rows = e.kw(0x6660, 1);
    const int at = e.kw(0x662a, 1) - 0x0ad4;  // where in the video card's memory, past the display
    const int row0 = at / 0x54, x0 = (at % 0x54) * 4;
    const int pull = static_cast<i8>((e.B(0x23a5) >> 1) - 3);
    const int down = pull < 0 ? 0 : pull, skip = pull < 0 ? -pull : 0;
    for (int y = 0; y < down; ++y)
      for (int x = 0; x < width; ++x) put(x0 + x, row0 + y, 0);
    for (int y = 0; y < rows - down; ++y)
      for (int x = 0; x < width; ++x) {
        const std::size_t i = static_cast<std::size_t>((skip + y) * width + x);
        if (i < data_.plungerImage.size()) put(x0 + x, row0 + down + y, data_.plungerImage[i]);
      }
  }
  // the ball, but for the dots of it that something on the table passes over (cs:95b0)
  if (ball_.valid()) {
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

  // the display, which the video card shows below the playfield though it comes first in its
  // memory: a dot is a byte of the first or the third plane, every other column
  const auto& video = e.videoMemory();
  for (int y = 0; y < kDisplayRows; ++y) {
    u8* out = frame + static_cast<std::size_t>(view + y) * kWidth;
    for (int x = 0; x < kWidth; ++x) out[x] = video[static_cast<std::size_t>(x & 3)][static_cast<std::size_t>(y * 0x54 + (x >> 2))];
  }
}

}  // namespace encore
