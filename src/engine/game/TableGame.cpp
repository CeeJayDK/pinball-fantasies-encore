#include "engine/game/TableGame.h"

#include "core/File.h"
#include "core/Log.h"
#include "engine/table/Gameshow.h"
#include "engine/table/PartyLand.h"
#include "engine/table/SpeedDevils.h"
#include "engine/table/StonesNBones.h"

namespace encore {
namespace {

std::unique_ptr<Engine> make(ByteView prg, int table) {
  switch (table) {
    case 0: return std::make_unique<PartyLand>(prg);
    case 1: return std::make_unique<SpeedDevils>(prg);
    case 2: return std::make_unique<Gameshow>(prg);
    default: return std::make_unique<StonesNBones>(prg);
  }
}

/// A key as the keyboard of the original's day says it: a code, after 0xe0 for the keys that
/// came later. Nought for a key the table has no use for.
struct Scancode {
  u8 code = 0;
  bool extended = false;
};

Scancode scancode(Key k) {
  // the letters, A to Z
  static constexpr u8 kLetters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                      0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
  if (k >= Key::A && k <= Key::Z) return {kLetters[static_cast<int>(k) - static_cast<int>(Key::A)]};
  if (k >= Key::F1 && k <= Key::F8) return {static_cast<u8>(0x3b + static_cast<int>(k) - static_cast<int>(Key::F1))};
  if (k >= Key::Digit1 && k <= Key::Digit8) return {static_cast<u8>(0x02 + static_cast<int>(k) - static_cast<int>(Key::Digit1))};
  switch (k) {
    case Key::ShiftLeft: return {0x2a};
    case Key::ShiftRight: return {0x36};
    case Key::ControlLeft: return {0x1d};
    case Key::ControlRight: return {0x1d, true};
    case Key::AltLeft: return {0x38};
    case Key::AltRight: return {0x38, true};
    case Key::Space: return {0x39};
    case Key::Enter: return {0x1c};
    case Key::Escape: return {0x01};
    case Key::ArrowDown: return {0x50, true};
    case Key::ArrowUp: return {0x48, true};
    case Key::ArrowLeft: return {0x4b, true};
    case Key::ArrowRight: return {0x4d, true};
    default: return {};
  }
}

}  // namespace

TableGame::TableGame(const std::filesystem::path& prg, ByteView module, int table, const Start& start)
    : options_(start.options), music_(48000), screen_(prg, table) {
  const auto program = file::readAll(prg);
  if (!program) throw DataError("cannot read " + prg.string());
  engine_ = make(*program, table);
  if (!music_.load(module)) throw DataError("the table's music is not a module");
  music_.setMono(options_.mono);
  music_.onJump = [this](u8 place) { return engine_->musicAsks(place); };
  engine_->sound = &music_;
  engine_->pollCallsMusic = false;
  engine_->refuseFewerPlayers = true;
  screen_.attach(*engine_);
  engine_->start(options_, start.bestScores);
  engine_->W(at::loopCounter) = start.chance;
  bestAtStart_ = bestScores();
}

TableGame::~TableGame() { music_.onJump = {}; }

void TableGame::key(Key key, bool down) {
  const Scancode s = scancode(key);
  if (s.code == 0 || left()) return;
  if (s.extended) engine_->key(0xe0);
  engine_->key(static_cast<u8>(s.code | (down ? 0 : 0x80)));
}

TableGame::Score TableGame::score(int player) const {
  Score s{};
  const u16 at = static_cast<u16>(engine_->A(0x021a) + player * engine_->kw(0x05d4, 1));
  for (u16 i = 0; i < 12; ++i) s[i] = engine_->nativeB(static_cast<u16>(at + i));
  return s;
}

TableGame::BestScores TableGame::bestScores() const {
  BestScores b{};
  const u16 at = engine_->kw(0x6377, 1);
  for (u16 i = 0; i < b.size(); ++i) b[i] = engine_->nativeB(static_cast<u16>(at + i));
  return b;
}

void TableGame::frame() {
  if (left()) return;
  try {
    engine_->frame();
  } catch (const std::exception& e) {
    // A routine of the original's that was not written, or one led astray: the table is left
    // rather than played on from nobody knows what.
    failure_ = e.what();
    log::error("table " + std::to_string(table() + 1) + ", frame " + std::to_string(frames_) + ": " + failure_);
  }
  music_.advance(1.0 / 60);
  ++frames_;

  const bool now = !waiting() && failure_.empty();
  if (playing_ && (!now || engine_->exited())) {
    Ended e;
    e.frame = frames_;
    // (the last ball's number is one past the balls there are, once it has been played)
    e.abandoned = engine_->exited() || !failure_.empty() || engine_->B(0x33dc) <= engine_->B(0x33dd);
    for (int p = 0; p < players(); ++p) e.scores.push_back(score(p));
    const BestScores best = bestScores();
    if (!e.scores.empty())
      for (std::size_t entry = 0; entry < 4 && e.initials[0] == 0; ++entry) {
        const u8* mine = best.data() + entry * 0x10;
        if (!std::equal(mine, mine + 12, e.scores[0].begin())) continue;
        bool before = false;
        for (std::size_t old = 0; old < 4; ++old) before |= std::equal(mine, mine + 15, bestAtStart_.data() + old * 0x10);
        if (!before) std::copy(mine + 12, mine + 15, e.initials.begin());
      }
    ended_.push_back(std::move(e));
    bestAtStart_ = best;
  }
  playing_ = now && !engine_->exited();
}

void TableGame::draw(u8* frame, Rgb* colours) const {
  screen_.draw(*engine_, frame, screenHeight());
  const auto& dac = engine_->colours();
  auto wide = [](u8 v) { return static_cast<u8>((v << 2) | (v >> 4)); };  // 0-63 as 0-255
  for (std::size_t i = 0; i < 256; ++i) colours[i] = Rgb{wide(dac[i * 3]), wide(dac[i * 3 + 1]), wide(dac[i * 3 + 2])};
}

}  // namespace encore
