#include "engine/game/TableGame.h"

#include <algorithm>

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

/// The best scores as the table keeps them, and as TABLEn.HI does: four of twelve digits,
/// three letters and a byte of nothing.
std::array<u8, 0x40> packed(const HighScores& scores) {
  std::array<u8, 0x40> out{};
  for (std::size_t i = 0; i < 4; ++i) {
    std::copy(scores[i].score.digits.begin(), scores[i].score.digits.end(), out.begin() + i * 0x10);
    std::copy(scores[i].name.begin(), scores[i].name.end(), out.begin() + i * 0x10 + 12);
  }
  return out;
}

bool same(const Options& a, const Options& b) {
  return a.balls == b.balls && a.angle == b.angle && a.scrollSpeed == b.scrollSpeed && a.resolution == b.resolution &&
         a.noMusic == b.noMusic && a.mono == b.mono;
}

bool same(const HighScores& a, const HighScores& b) {
  for (std::size_t i = 0; i < 4; ++i)
    if (a[i].score != b[i].score || a[i].name != b[i].name) return false;
  return true;
}

}  // namespace

TableGame::TableGame(ByteView prg, ByteView module, int table, const Setup& setup)
    : options_(setup.options), engineHigh_(setup.options.resolution != Resolution::Normal), music_(48000) {
  engine_ = make(prg, table);
  if (setup.picture) screen_ = std::make_unique<TableScreen>(Bytes(prg.begin(), prg.end()), table);
  if (!music_.load(module)) throw DataError("the table's music is not a module");
  music_.setMono(options_.mono);
  music_.onJump = [this](u8 place) { return engine_->musicAsks(place); };
  engine_->sound = &music_;
  engine_->pollCallsMusic = false;
  engine_->refuseFewerPlayers = true;
  if (screen_) screen_->attach(*engine_);

  Engine::Options o;
  o.fiveBalls = options_.balls == 5;
  o.lowAngle = options_.angle == Angle::Low;
  o.scrolling = static_cast<u8>(options_.scrollSpeed);
  o.musicOff = options_.noMusic;
  o.highResolution = engineHigh_;
  o.mono = options_.mono;
  const auto best = packed(setup.highScores);
  engine_->start(o, best);
  if (options_.angle == Angle::Higher) engine_->setAngle(2);
  engine_->W(at::loopCounter) = setup.chance;
  // what the table this one's game was started on had that the game would not be the same without
  const Recording::Carry& carry = setup.carry;
  if (carry.balls != 0) engine_->B(0x33dd) = carry.balls;
  if (carry.noTilt) engine_->B(0x372a) = 0xff;
  if (carry.otherSteps) engine_->B(at::keys) ^= 4;
  if (carry.scrollPos != 0xffff) {
    engine_->W(0x3383) = carry.scrollPos;
    engine_->W(0x2f04) = carry.scrollAt;
  }
  if (screen_) screen_->started(*engine_);
  camera_ = (static_cast<i16>(engine_->screenRow()) - TableScreen::kDisplayRows) * 16;

  recording_.table = table;
  recording_.chance = setup.chance;
  recording_.options = options_;
  recording_.highScores = setup.highScores;
  recording_.carry = carry;
  recording_.carry.balls = engine_->B(0x33dd);
  bestAtStart_ = savedScores_ = highScores();
  saved_ = options();
}

TableGame::~TableGame() { music_.onJump = {}; }

Bcd TableGame::score(int player) const {
  Bcd s;
  const u16 at = static_cast<u16>(engine_->A(0x021a) + player * engine_->kw(0x05d4, 1));
  for (u16 i = 0; i < 12; ++i) s.digits[i] = engine_->nativeB(static_cast<u16>(at + i));
  return s;
}

HighScores TableGame::highScores() const {
  HighScores h{};
  const u16 at = engine_->kw(0x6377, 1);
  for (u16 i = 0; i < 4; ++i) {
    for (u16 d = 0; d < 12; ++d) h[i].score.digits[d] = engine_->nativeB(static_cast<u16>(at + i * 0x10 + d));
    for (u16 c = 0; c < 3; ++c) h[i].name[c] = engine_->nativeB(static_cast<u16>(at + i * 0x10 + 12 + c));
  }
  return h;
}

Options TableGame::options() const {
  Options o = options_;
  o.angle = engine_->angle() == 0 ? Angle::Low : engine_->angle() == 2 ? Angle::Higher : Angle::High;
  o.noMusic = engine_->musicIsOff();
  return o;
}

bool TableGame::optionsChanged() { return std::exchange(optionsChanged_, false); }
bool TableGame::highScoresChanged() { return std::exchange(scoresChanged_, false); }

Recording::Carry TableGame::carryOver() const {
  Recording::Carry c;
  c.noTilt = engine_->B(0x372a) == 0xff;
  c.otherSteps = ((engine_->B(at::keys) & 4) != 0) != engineHigh_;
  c.balls = engine_->B(0x33dd);
  c.scrollPos = engine_->W(0x3383);
  c.scrollAt = engine_->W(0x2f04);
  return c;
}

bool TableGame::startsGame(Key key) const {
  if (!waiting() || left() || asking_) return false;
  if (key != Key::Enter && !(key >= Key::F1 && key <= Key::F8)) return false;
  // the table takes keys, more players may join, and it is not asking whether to be left
  return engine_->CB(0x3475) != 0 && engine_->B(0x33e3) != 0 && engine_->CB(0x3195) != 0xff;
}

bool TableGame::askingName() const { return engine_->W(0x33e7) == engine_->F(0x0609); }

void TableGame::key(Key key, bool down) {
  if (left()) return;
  recording_.events.push_back({frames_, down, key});
  if (key == Key::ArrowUp) up_ = down;
  if (key == Key::ArrowDown) down_ = down;
  if (asking_) {  // this version's question, which the table knows nothing of
    if (!down) return;
    if (key == Key::Y) sendOnline_ = true;
    if (key == Key::Y || key == Key::N || key == Key::Escape) {
      asking_ = false;
      engine_->write("");
    }
    return;
  }
  // The original's pause ends with any key but escape. Here it ends with P; escape asks
  // whether to leave, as there; and other keys change the options, or do nothing.
  if (down && engine_->isPaused() && !engine_->asksToQuit() && key != Key::P && key != Key::Escape) return pausedKey(key);
  const Scancode s = scancode(key);
  if (s.code == 0) return;
  if (s.extended) engine_->key(0xe0);
  engine_->key(static_cast<u8>(s.code | (down ? 0 : 0x80)));
}

void TableGame::pausedKey(Key key) {
  switch (key) {
    case Key::A:
      engine_->setAngle(engine_->angle() == 1 ? 2 : engine_->angle() == 2 ? 0 : 1);
      engine_->write(engine_->angle() == 0 ? "ANGLE LOW" : engine_->angle() == 2 ? "ANGLE HIGHER" : "ANGLE HIGH");
      break;
    case Key::S:
      options_.scrollSpeed = static_cast<ScrollSpeed>((static_cast<int>(options_.scrollSpeed) + 1) % 3);
      engine_->setScrolling(static_cast<u8>(options_.scrollSpeed));
      engine_->write(options_.scrollSpeed == ScrollSpeed::Hard ? "SCROLLING HARD" : options_.scrollSpeed == ScrollSpeed::Soft ? "SCROLLING SOFT" : "SCROLLING MEDIUM");
      break;
    case Key::M:
      engine_->toggleMusic();
      engine_->write(engine_->musicIsOff() ? "MUSIC OFF" : "MUSIC ON");
      break;
    case Key::R: {
      const int before = viewTop();
      options_.resolution = static_cast<Resolution>((static_cast<int>(options_.resolution) + 1) % 3);
      camera_ = before * 16;
      engine_->write(options_.resolution == Resolution::Normal ? "RESOLUTION NORMAL" : options_.resolution == Resolution::High ? "RESOLUTION HIGH" : "RESOLUTION FULL");
      break;
    }
    case Key::F7:
      lamps_ = (lamps_ + 1) % 3;
      break;
    default: break;
  }
}

int TableGame::screenHeight() const {
  return options_.resolution == Resolution::Full ? TableData::kHeight + TableScreen::kDisplayRows
                                                 : TableScreen::height(options_.resolution == Resolution::High);
}

int TableGame::viewRows() const { return screenHeight() - TableScreen::kDisplayRows; }

int TableGame::viewTop() const {
  if (options_.resolution == Resolution::Full) return 0;
  const bool own = (options_.resolution == Resolution::High) == engineHigh_;
  const int top = own ? static_cast<i16>(engine_->screenRow()) - TableScreen::kDisplayRows
                      : (camera_ >> 4) + engine_->W(at::nudgeLift).s();
  if (manual_ == 0) return top;
  return std::clamp(top + manual_, 0, TableData::kHeight - viewRows());
}

/// Where the screen looks when it is not of the size the table was started for: as the
/// original's own (cs:4018), with the other size's numbers.
void TableGame::follow() {
  if (options_.resolution == Resolution::Full || (options_.resolution == Resolution::High) == engineHigh_) return;
  const bool high = options_.resolution == Resolution::High;
  const int lead = high ? 0x82 : 0x4b, last = high ? 0x103 : 0x171, band = high ? 0xaa : 0x73;
  const int tableLead = engineHigh_ ? 0x82 : 0x4b, tableLast = engineHigh_ ? 0x103 : 0x171;
  Engine& e = *engine_;
  if (e.W(0x3383) != 0xffff) {  // the table says where: the same way down the picture
    camera_ = (e.W(0x3383).s() * last / tableLast) * 16;
    return;
  }
  int row = std::clamp(e.W(at::ballY).s() - lead, 0, last);
  if (e.W(0x3385) != 0xffff) row = std::clamp(e.W(0x3385).s() + tableLead - lead, 0, last);
  camera_ += ((row - (camera_ >> 4)) * e.W(0x23ac).s()) >> 2;
  int off = row - (camera_ >> 4);
  if (off >= 0) {
    off -= band;
    if (off >= 0) camera_ += off << 4;
  } else {
    off += lead;
    if (off <= 0) camera_ += off << 4;
  }
}

void TableGame::frame() {
  if (left()) return;
  ++frames_;
  recording_.frames = frames_;
  music_.advance(1.0 / 60);
  if (engine_->exited()) {
    // cs:3a11: the table is left: its picture and its music fade away, in 128 frames
    leaving_ -= 2;
    if (leaving_ >= 0 && (leaving_ & 0x0f) == 0) music_.volume(static_cast<u16>(leaving_));
    return;
  }
  if (asking_) return;  // the table waits for the answer
  try {
    engine_->frame();
  } catch (const std::exception& e) {
    // A routine of the original's that was not written, or one led astray: the table is left
    // rather than played on from nobody knows what.
    failure_ = e.what();
    log::error("table " + std::to_string(table() + 1) + ", frame " + std::to_string(frames_) + ": " + failure_);
  }
  if (engine_->isPaused()) {
    if (up_) manual_ -= 4;
    if (down_) manual_ += 4;
    const int top = viewTop() - manual_;
    manual_ = std::clamp(manual_, -top, std::max(-top, TableData::kHeight - viewRows() - top));
  } else {
    manual_ = 0;
    lamps_ = 0;
    follow();
  }

  // In a game of one player, the initials for a best score are in (the display waits a second
  // on them, cs:06c0): this version then asks whether the game is to be sent online.
  const u16 wait = engine_->W(0x33e7);
  if (wait != lastWait_ && wait == engine_->F(0x06c0) && players() == 1 && !recording_.cheated() && failure_.empty()) {
    asking_ = true;
    engine_->write("SEND ONLINE \\Y OR N]");
  }
  lastWait_ = wait;

  const bool now = !waiting() && failure_.empty() && !engine_->exited();
  if (playing_ && !now) {
    Recording::Game g;
    g.endFrame = frames_;
    // (the last ball's number is one past the balls there are, once it has been played)
    g.abandoned = engine_->exited() || !failure_.empty() || engine_->B(0x33dc) <= engine_->B(0x33dd);
    for (int p = 0; p < players(); ++p) g.scores.push_back(score(p));
    const HighScores best = highScores();
    if (!g.scores.empty())
      for (std::size_t entry = 0; entry < 4 && g.initials[0] == 0; ++entry) {
        if (best[entry].score != g.scores[0]) continue;
        bool before = false;
        for (const HighScore& old : bestAtStart_) before |= old.score == best[entry].score && old.name == best[entry].name;
        if (!before) g.initials = best[entry].name;
        // (the table keeps a space typed as its own mark for one)
        for (u8& c : g.initials) c = c == '*' ? ' ' : c;
      }
    recording_.games.push_back(std::move(g));
    bestAtStart_ = best;
  }
  playing_ = now;

  if (const Options o = options(); !same(o, saved_)) {
    saved_ = o;
    optionsChanged_ = true;
  }
  if (const HighScores h = highScores(); !same(h, savedScores_)) {
    savedScores_ = h;
    scoresChanged_ = true;
  }
}

void TableGame::draw(u8* frame, Rgb* colours, HdFrame* hd) const {
  if (!screen_) return;
  TableScreen::View view;
  view.height = screenHeight();
  view.top = viewTop();
  view.lamps = lamps_;
  view.ballTrail = ballTrail;
  screen_->draw(*engine_, frame, view, hd);
  screen_->colours(*engine_, colours, lamps_);
  if (engine_->exited()) {  // cs:4f3c: every colour by how bright the table still is
    const int level = std::max(leaving_, 0);
    for (std::size_t i = 0; i < 256; ++i)
      colours[i] = Rgb{static_cast<u8>(colours[i].r * level >> 8), static_cast<u8>(colours[i].g * level >> 8), static_cast<u8>(colours[i].b * level >> 8)};
    if (hd) {
      hd->fade.fill(static_cast<float>(level) / 256.0f);
      hd->spriteTint = static_cast<float>(level) / 256.0f;
    }
  }
}

void TableGame::sound(float* out, int frames) {
  music_.render(out, frames);
  if (silent) std::fill_n(out, static_cast<std::size_t>(frames) * 2, 0.0f);
}

}  // namespace encore
