#include "engine/game/Play.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "core/Log.h"

namespace encore {
namespace {

/// The shooter lane, where a new ball appears before it is launched.
constexpr int kServeX = 297;
constexpr int kServeY = 530;
constexpr int kMaxPull = 32;

// The camera keeps the ball this far below the top of the window, eases towards that
// target in proportion to the configured scroll speed, and is rubber-banded so it can
// never drift far from where it should be.
constexpr int kCameraLeadHigh = 130;
constexpr int kCameraLeadNormal = 75;
constexpr int kCameraAheadHigh = 130;
constexpr int kCameraAheadNormal = 75;
constexpr int kCameraBehindHigh = 170;
constexpr int kCameraBehindNormal = 115;

/// Where the plunger is drawn, and how far the pull moves it down the screen.
constexpr int kPlungerX = 304;
constexpr int kPlungerRow = 556;

u32 nextRandom(u32& state) {
  state = state * 1103515245u + 12345u;
  return (state >> 16) & 0x7fff;
}

}  // namespace

void Play::init(const TableData& table, bool highResolution) {
  table_ = &table;
  highResolution_ = highResolution;
  physics_.init(table, highResolution);
  tableIndex_ = table.index;
  bumpers_ = extractBumpers(table.dataSegment, table.index);
  patches_ = extractPhysmapPatches(table.dataSegment, table.index);
  // Gates the rules raise when a game starts.
  if (table.index == 3) {
    physics_.setPatch(patches_[static_cast<std::size_t>(StonesGate::TowerEntry)], true);
    physics_.setPatch(patches_[static_cast<std::size_t>(StonesGate::Kickback)], true);
  }
  physics_.setBumpers(&bumpers_);
  ballSprite_ = BallSprite::decode(table.code);
  if (!ballSprite_.valid()) log::warn(table.name + ": the ball sprite could not be decoded");
  flipperGraphics_ = extractFlipperGraphics(table);
  lights_ = extractLights(table.dataSegment, table.index);
  lit_.assign(lights_.size(), false);
  triggers_ = extractTriggers(table.dataSegment, table.index);
  sounds_ = extractSounds(table.dataSegment, table.index);
  dmAssets_ = extractDmAssets(table.dataSegment, table.index);
  dmd_.setAssets(&dmAssets_);
  dmd_.clear();
  scoreChanged_ = true;
  log::info(table.name + ": " + std::to_string(flipperGraphics_.size()) + " flippers, " +
            (flipperGraphics_.empty() ? "0" : std::to_string(flipperGraphics_[0].frames.size())) +
            " frames each, " + std::to_string(lights_.size()) + " lights");
  serveBall();
}

void Play::playSfx(const Sfx& sfx) const {
  if (!player_ || !sfx.valid()) return;
  player_->triggerNote(sfx.channel, sfx.sample, sfx.note, 64);
}

void Play::playMusic(const Jingle& jingle) const {
  if (!player_ || !jingle.valid()) return;
  player_->setPosition(jingle.position);
  player_->play();
}

void Play::serveBall() {
  ball_ = Ball{};
  ball_.place(kServeX, kServeY);
  ball_.active = true;
  plungerPull_ = 0;
  launched_ = false;
  cameraRow_ = TableData::kHeight - viewRows();
  cameraFixed_ = cameraRow_ << 4;
  playSfx(sounds_.issueBall);
  playMusic(sounds_.plunger);
}

void Play::update(const PlayInput& input, double dt) {
  (void)dt;
  if (input.serve && !ball_.active) serveBall();

  // The plunger charges while the key is held and fires the moment it is let go.
  if (input.plunger) {
    plungerPull_ = std::min(plungerPull_ + 1, kMaxPull);
  } else if (plungerWasHeld_ && plungerPull_ > 0) {
    physics_.launch(ball_, plungerPull_, nextRandom(random_));
    // The spring's sound gets louder the further it was drawn back.
    Sfx spring = sounds_.spring;
    if (spring.valid() && player_) player_->triggerNote(spring.channel, spring.sample, spring.note,
                                                       std::min(64, plungerPull_ * 2));
    plungerPull_ = 0;
    launched_ = true;
    playMusic(sounds_.main);
  }
  plungerWasHeld_ = input.plunger;

  // The flippers click as they are pressed, not while held.
  if ((input.leftFlipper && !leftWasHeld_) || (input.rightFlipper && !rightWasHeld_)) playSfx(sounds_.flipper);
  leftWasHeld_ = input.leftFlipper;
  rightWasHeld_ = input.rightFlipper;

  Physics::Controls controls;
  controls.leftFlipper = input.leftFlipper;
  controls.rightFlipper = input.rightFlipper;
  controls.nudge = input.nudge;

  events_.clear();
  physics_.clearContact();
  physics_.beginFrame(ball_);
  for (int i = 0; i < physics_.subStepsPerFrame(); ++i) physics_.subStep(ball_, controls);
  collectTriggers();
  refreshDisplay();
  if (ball_.lost) {
    ball_.active = false;
    ball_.lost = false;
    playSfx(sounds_.drained);
    playMusic(sounds_.drainedJingle);
  }
  updateCamera();
}

void Play::setLight(std::size_t index, bool lit) {
  if (index < lit_.size()) lit_[index] = lit;
}

bool Play::lightLit(std::size_t index) const { return index < lit_.size() && lit_[index]; }

void Play::applyPalette(Palette& palette) const {
  if (!table_) return;
  palette.set(0, table_->palette);
  // The display's lit colour is a single palette entry, which the original also uses to
  // blink the whole display at once.
  palette[dmAssets_.palette.indexOn] = dmAssets_.palette.colorOn;
  for (std::size_t i = 0; i < lights_.size(); ++i) {
    const TableLight& light = lights_[i];
    const bool on = i < lit_.size() && lit_[i];
      for (std::size_t c = 0; c < light.colors.size(); ++c) {
      const std::size_t entry = light.baseIndex + c;
      if (entry >= 256) break;
      const Rgb& colour = light.colors[c];
      palette[entry] = on ? colour
                          : Rgb{static_cast<u8>(colour.r / 2), static_cast<u8>(colour.g / 2),
                                static_cast<u8>(colour.b / 2)};
    }
  }
}

/// Until the script system drives the display, it shows the running score, which is what the
/// original shows for most of a ball anyway.
void Play::refreshDisplay() {
  if (!scoreChanged_) return;
  scoreChanged_ = false;
  dmd_.clear();
  const std::string text = score_.text();
  dmd_.drawText(Dmd::kDotsWide - Dmd::kCellWidth * static_cast<int>(text.size()) - 2, 1, text,
                DmFontSize::Height13);
}

/// Two rollovers move the ball down to the playfield from their handler rather than through
/// a transition zone: the end of Speed Devils' plunger lane, which also stops the ball's fall
/// back into the lane, and the end of Stones 'n Bones' top rail, which drops it into the key lanes.
void Play::applyLayerHandler(u16 handler) {
  // Speed Devils exists in builds whose code is laid out differently; both addresses are seen.
  if (tableIndex_ == 1 && (handler == 0x11bb || handler == 0x11d1)) {
    ball_.vy = 0;
    ball_.upper = false;
  } else if (tableIndex_ == 3 && handler == 0x1738) {
    ball_.upper = false;
  }
}

/// Works out what the ball touched or rolled over this frame.
void Play::collectTriggers() {
  if (!ball_.active) return;
  // A hit trigger fires when the ball collided with something solid inside its rectangle.
  // A bumper or kicker scores and sounds on its own, without a table handler.
  if (const int hit = physics_.takeBumper(); hit >= 0 && hit < static_cast<int>(bumpers_.size())) {
    const Bumper& b = bumpers_[static_cast<std::size_t>(hit)];
    playSfx(b.sfx);
    score_.add(b.score);
    scoreChanged_ = true;
  }
  if (physics_.touched()) {
    const Point p = physics_.contactPoint();
    for (const TriggerArea& area : triggers_.hit)
      if (area.rect.contains(p.x, p.y)) {
        events_.push_back({false, area.handler});
        break;
      }
  }
  // A roll trigger fires once, as the ball's centre enters its rectangle.
  const int cx = ball_.x + 8;
  const int cy = ball_.y + 8;
  u16 inside = 0;
  for (const TriggerArea& area : triggers_.roll(ball_.upper, physics_.tilted()))
    if (area.rect.contains(cx, cy)) {
      inside = area.handler;
      break;
    }
  if (inside != 0 && inside != insideRoll_) {
    events_.push_back({true, inside});
    playSfx(sounds_.rollInner);
    applyLayerHandler(inside);
  }
  insideRoll_ = inside;
}

void Play::updateCamera() {
  const int maxRow = TableData::kHeight - viewRows();
  const int lead = highResolution_ ? kCameraLeadHigh : kCameraLeadNormal;
  const int ahead = highResolution_ ? kCameraAheadHigh : kCameraAheadNormal;
  const int behind = highResolution_ ? kCameraBehindHigh : kCameraBehindNormal;
  const int target = std::clamp(ball_.y - lead, 0, maxRow);
  // A first-order lag, so the view eases rather than snapping.
  cameraFixed_ += (target - (cameraFixed_ >> 4)) * scrollSpeed_ / 4;
  int row = cameraFixed_ >> 4;
  if (row - target > ahead) row = target + ahead;
  if (target - row > behind) row = target - behind;
  cameraFixed_ = row << 4;
  cameraRow_ = std::clamp(row, 0, maxRow);
}

void Play::render(Framebuffer& frame) const {
  if (frame.width() != kWidth || frame.height() != height()) frame = Framebuffer(kWidth, height());
  frame.clear(0);
  if (!table_) return;

  // The playfield window.
  for (int y = 0; y < viewRows(); ++y) {
    const int source = cameraRow_ + y;
    if (source < 0 || source >= TableData::kHeight) continue;
    std::memcpy(frame.row(y), table_->playfield.data() + static_cast<std::size_t>(source) * TableData::kWidth,
                kWidth);
  }

  dmd_.render(frame);

  // The flippers, each drawn at whichever step of its travel it has reached.
  for (std::size_t i = 0; i < flipperGraphics_.size() && i < physics_.flippers().size(); ++i) {
    const FlipperFrames& g = flipperGraphics_[i];
    if (!g.valid()) continue;
    const int step = std::clamp(physics_.flippers()[i].frame, 0, static_cast<int>(g.frames.size()) - 1);
    const Bytes& picture = g.frames[static_cast<std::size_t>(step)];
    const int top = g.y - cameraRow_;
    for (int row = 0; row < g.height; ++row)
      for (int column = 0; column < g.width; ++column)
        frame.put(g.x + column, top + row, picture[static_cast<std::size_t>(row) * g.width + column]);
  }

  // The plunger, which slides down the screen as it is drawn back.
  if (table_->plungerImage.size() >= kPlungerWidth * kPlungerHeight) {
    const int row = kPlungerRow + (plungerPull_ >> 1) - 3 - cameraRow_;
    for (int y = 0; y < kPlungerHeight; ++y)
      for (int x = 0; x < kPlungerWidth; ++x) {
        const u8 c = table_->plungerImage[static_cast<std::size_t>(y) * kPlungerWidth + x];
        if (c != 0) frame.put(kPlungerX + x, row + y, c);
      }
  }

  // The ball, drawn from the sprite recovered out of the original's own draw routine. It is
  // hidden wherever the occlusion map for its layer says a playfield feature passes over it,
  // which is what lets it disappear under the ramps.
  if (ball_.active && ballSprite_.valid()) {
    const Mask& occluded = ball_.upper ? table_->occlusionOverhead : table_->occlusionGround;
    for (int y = 0; y < ballSprite_.height; ++y) {
      const int tableY = ball_.y + y;
      for (int x = 0; x < ballSprite_.width; ++x) {
        if (!ballSprite_.covers(x, y)) continue;
        const int tableX = ball_.x + x;
        if (occluded.get(tableX, tableY)) continue;
        frame.put(tableX, tableY - cameraRow_, ballSprite_.at(x, y));
      }
    }
  }
}

}  // namespace encore
