// Ball, flipper and collision physics (translated from pfr's src/table/physics.rs and ball.rs).
#include <algorithm>
#include <cstdlib>

#include "table/Table.h"

namespace pfr {

namespace {
constexpr std::size_t kMaterialFlipper = 2;
constexpr std::size_t kMaterialKicker = 3;
i16 clampSpeed(i32 v, i16 max) { return static_cast<i16>(std::clamp<i32>(v, -max, max)); }
}  // namespace

/// The Higher angle adds 3 to the playfield's downward pull of 8 (so 11/8 of it). A ball
/// needs sqrt(11/8), about 1.17 times the speed to climb as high; playing it showed the
/// flippers and plunger want a little more than that, so at that angle they, and the speed
/// limit, are scaled by 125/100.
i16 Table::angleBoost(i32 v) const {
  return static_cast<i16>(options_.angle == Angle::Higher ? v * 125 / 100 : v);
}

namespace {
}  // namespace

void Table::ballTeleportFreeze(Layer layer, std::array<i16, 2> pos) {
  ball_.layer = layer;
  ball_.setPos(pos);
  ball_.speed = {0, 0};
  ball_.frozen = true;
}

void Table::ballTeleport(Layer layer, std::array<i16, 2> pos, std::array<i16, 2> speed) {
  ball_.layer = layer;
  ball_.setPos(pos);
  ball_.speed = speed;
  ball_.frozen = false;
  const i16 random = static_cast<i16>(rand(0x400));
  ball_.rotation = (random & 1) ? static_cast<i16>(-random) : random;
}

void Table::physicsFrame() {
  if (ball_.frozen) {
    push_.frame(spaceState_);
    flippersMove();
    flippersPhysmapUpdate();
  } else {
    if (auto c = physicsCheckCollision()) physicsNewDir(*c);
    push_.frame(spaceState_);
    flippersMove();
    ballMove();
    flippersPhysmapUpdate();
  }
}

std::optional<Table::Collision> Table::physicsCheckCollision() {
  u16 angleSum = 0, cnt = 0;
  u8 quad = 0;
  u16 ctrB = 0;
  std::optional<std::size_t> material;
  const auto pos = ball_.pos();
  const Grid8& map = physmaps_[static_cast<std::size_t>(ball_.layer)];
  for (const BallOutlinePixel& pix : assets_.ballOutline) {
    const int x = pos[0] + pix.x - 1;
    const int y = pos[1] + push_.offset() + pix.y - 1;
    if (y < 0 || y >= 576 || x < 0 || x >= 320) continue;
    const u8 byte = map(x, y);
    if (byte & 2) {
      angleSum = static_cast<u16>(angleSum + pix.angle);
      quad |= pix.quad;
      if (pix.isBot) ++ctrB;
      material = byte & 7;
      ++cnt;
    }
  }
  if (cnt == 0) return std::nullopt;
  if (quad == 0xb || quad == 9 || quad == 0xd) angleSum = static_cast<u16>(angleSum + (ctrB << 11));
  const u16 angle = static_cast<u16>((angleSum / cnt) & 0x7ff);
  const std::size_t idx = (static_cast<std::size_t>(angle) * 0x580 + 0x8000) >> 16;
  const auto hp = assets_.ballOutlineByAngle[idx % 44];
  const std::array<i16, 2> hitPos = {static_cast<i16>(hp[0] + pos[0]), static_cast<i16>(hp[1] + pos[1])};
  hitPos_ = hitPos;
  std::array<i16, 2> flipperSpeed{};
  if (*material == kMaterialFlipper) {
    for (std::size_t f = 0; f < assets_.flippers.size(); ++f) {
      const Flipper& fl = assets_.flippers[f];
      if (!fl.ballBbox.contains(hitPos[0], hitPos[1])) continue;
      i16 dx = static_cast<i16>(hitPos[0] - fl.originX);
      i16 dy = static_cast<i16>(hitPos[1] - fl.originY);
      if (fl.side == FlipperSide::Left) {
        if (dx < 0) continue;
      } else {
        if (dx >= 0) continue;
        if (!fl.isVertical) {
          dx = static_cast<i16>(-dx);
          dy = static_cast<i16>(-dy);
        }
      }
      i16 extra;
      if (fl.isVertical) {
        std::swap(dx, dy);
        extra = static_cast<i16>(std::abs(dy >> 1));
      } else {
        extra = static_cast<i16>(std::abs(dy) >> 2);
      }
      const i16 s = flippers_[f].speed;
      flipperSpeed = {angleBoost(dy * -s), angleBoost(-(dx + extra) * -s)};
    }
  } else if ((*material == 3 || *material == 7) && !tilted_) {
    for (std::size_t b = 0; b < assets_.bumpers.size(); ++b) {
      const Bumper& bu = assets_.bumpers[b];
      if (bu.isKicker != (*material == 3)) continue;
      if (bu.rect.contains(hitPos[0], hitPos[1])) hitBumper_ = b;
    }
  }
  return Collision{flipperSpeed, angle, *material, cnt};
}

void Table::physicsNewDir(const Collision& c) {
  const i16 maxSpeed = angleBoost(ball_.maxSpeed);
  const Material& m = materials_[c.material];
  const std::array<i16, 2> speed = {clampSpeed(ball_.speed[0] + c.flipperSpeed[0], maxSpeed),
                                    clampSpeed(ball_.speed[1] + c.flipperSpeed[1] + push_.speed, maxSpeed)};
  const std::size_t a = static_cast<std::size_t>((0x800 - c.angle) & 0x7ff);
  i32 cos = assets_.sineTable[a + 0x200], sin = assets_.sineTable[a];
  i32 dot = (speed[0] * cos - speed[1] * sin) >> 13;
  i32 cross = (speed[0] * sin + speed[1] * cos) >> 13;
  if (dot <= 0) {
    hitBumper_.reset();
    return;
  }
  if (dot <= m.minBounceSpeed) {
    dot = 0;
    hitBumper_.reset();
  } else {
    const i16 bounceFactor = static_cast<i16>(std::abs((cross * 0x10) / dot));
    if (bounceFactor < m.maxBounceAngle) {
      if (hitBumper_) {
        if (c.material == kMaterialKicker) {
          if (dot < kickerSpeedThreshold_)
            hitBumper_.reset();
          else
            dot += kickerSpeedBoost_;
        } else {
          dot += bumperSpeedBoost_;
        }
      }
    } else {
      dot = 0;
      hitBumper_.reset();
    }
  }
  dot -= dot * 256 / m.bounceFactor;
  i32 cx = m.unk0, bp = m.unk2;
  if (dot < 1024) {
    const i32 factor = (dot >> 6) + 1;
    cx *= factor;
    bp *= factor;
  }
  dot = -dot;
  const i32 rot = ball_.rotation + push_.speed - cross;
  cross += rot * 256 / cx;
  ball_.rotation = static_cast<i16>(ball_.rotation - static_cast<i16>(rot * 256 / bp));
  cross = cross * 0x800 / 0x801;
  cos = assets_.sineTable[0x200 + c.angle];
  sin = assets_.sineTable[c.angle];
  i16 speedX = static_cast<i16>((dot * cos - cross * sin) >> 15);
  i16 speedY = static_cast<i16>((dot * sin + cross * cos) >> 15);
  speedX = static_cast<i16>(speedX - c.flipperSpeed[0]);
  speedY = static_cast<i16>(speedY - c.flipperSpeed[1]);
  speedY = static_cast<i16>(speedY - push_.speed);
  ball_.speed = {clampSpeed(speedX, maxSpeed), clampSpeed(speedY, maxSpeed)};
  if (c.cnt >= 6) {
    ball_.posHires[0] += -cos >> 6;
    ball_.posHires[1] += -sin >> 6;
  }
}

void Table::ballMove() {
  ball_.posHires[0] += ball_.speed[0];
  ball_.posHires[1] += ball_.speed[1];
  if (ball_.pos()[1] >= 576) drained_ = true;
  ball_.speed[0] = static_cast<i16>(ball_.speed[0] + ball_.accel[0]);
  ball_.speed[1] = static_cast<i16>(ball_.speed[1] + ball_.accel[1]);
  if (ball_.rotation < 0) {
    ball_.rotation = static_cast<i16>(ball_.rotation + 2);
    if (ball_.rotation > 0) ball_.rotation = 0;
  } else {
    ball_.rotation = static_cast<i16>(ball_.rotation - 2);
    if (ball_.rotation < 0) ball_.rotation = 0;
  }
}

void Table::springRelease() {
  if (atSpring_) {
    const i16 factor = hifps_ ? -166 : -138;
    ball_.speed = {0, angleBoost(factor * springPos_ - rand(0x100))};
    ball_.rotation = static_cast<i16>(rand(0x10));
  }
  playSfxBind(SfxBind::SpringUp, static_cast<u8>(springPos_ * 2));
  springPos_ = 0;
}

void Table::flippersMove() {
  for (std::size_t f = 0; f < assets_.flippers.size(); ++f) {
    const Flipper& fl = assets_.flippers[f];
    FlipperState& s = flippers_[f];
    if (flipperState_[static_cast<std::size_t>(fl.side)] && flippersEnabled_)
      s.speed = std::max<i16>(static_cast<i16>(s.speed + s.accelPress), s.speedPressStart);
    else
      s.speed = static_cast<i16>(s.speed + s.accelRelease);
    s.pos = static_cast<i16>(s.pos + s.speed);
    if (s.pos < 55) {
      s.pos = 0;
      s.speed = 0;
    }
    if (s.pos > fl.posMax) {
      s.pos = fl.posMax;
      s.speed = 0;
    }
    s.quantum = static_cast<u16>(s.pos / 55);
  }
}

void Table::physmapPatch(Layer layer, int x, int y, const Grid8& src) {
  physmaps_[static_cast<std::size_t>(layer)].paste(x, y, src);
}

void Table::flippersPhysmapUpdate() {
  for (std::size_t f = 0; f < assets_.flippers.size(); ++f) {
    FlipperState& s = flippers_[f];
    if (s.quantum == s.prevQuantum) continue;
    s.prevQuantum = s.quantum;
    const Flipper& fl = assets_.flippers[f];
    physmapPatch(Layer::Ground, fl.rectX, fl.rectY, fl.physmap[s.quantum]);
  }
}

void Table::dropPhysmap(PhysmapBind bind) {
  const auto& p = assets_.physmapPatches[static_cast<std::size_t>(bind)];
  physmapPatch(p->layer, p->x, p->y, p->dropped);
}

void Table::raisePhysmap(PhysmapBind bind) {
  const auto& p = assets_.physmapPatches[static_cast<std::size_t>(bind)];
  physmapPatch(p->layer, p->x, p->y, p->raised);
}

void Table::ballGravity() {
  const auto p = ball_.pos();
  const int cx = p[0] + 8, cy = p[1] + 8;
  if (cy < 0 || cy >= 576 || cx < 0 || cx >= 320) return;
  const std::size_t ramp = physmaps_[static_cast<std::size_t>(ball_.layer)](cx, cy) >> 4;
  if (ramp == 0xf || ramp >= assets_.ramps.size()) return;
  ball_.accel = hifps_ ? assets_.ramps[ramp].accelHires : assets_.ramps[ramp].accel;
  if (options_.angle == Angle::Low) ball_.accel[1] = static_cast<i16>(ball_.accel[1] - 3);
  if (options_.angle == Angle::Higher) ball_.accel[1] = static_cast<i16>(ball_.accel[1] + 3);
}

std::array<i16, 2> Table::ballCenter() const {
  const auto p = ball_.pos();
  return {static_cast<i16>(p[0] + 8), static_cast<i16>(p[1] + 8 + push_.offset())};
}

void Table::checkTransitions() {
  const auto c = ballCenter();
  const auto& list = ball_.layer == Layer::Ground ? assets_.transitionsUp : assets_.transitionsDown;
  for (const TRect& r : list)
    if (r.contains(c[0], c[1])) {
      ball_.layer = ball_.layer == Layer::Ground ? Layer::Overhead : Layer::Ground;
      return;
    }
}

void Table::scoreBumper() {
  if (!hitBumper_) return;
  const Bumper& b = assets_.bumpers[*hitBumper_];
  hitBumper_.reset();
  player_->playSfx(b.sfx, 0x40);
  score(b.score, Bcd::kZero);
  modeCountHit();
}

}  // namespace pfr
