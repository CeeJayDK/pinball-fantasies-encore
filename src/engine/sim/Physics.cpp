#include "engine/sim/Physics.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>

#include "core/Error.h"

namespace encore {
namespace {

/// The 44 probe points on the radius-8 circle, with the angle each one represents
/// in units of 1/2048 of a turn. Angle 0 points right, 0x200 points down.
struct ProbePoint {
  i8 dx, dy;
  i16 angle;
};
constexpr ProbePoint kProbes[44] = {
    {8, 0, 0x000},  {8, 1, 0x029},   {8, 2, 0x050},   {7, 3, 0x084},   {7, 4, 0x0a9},   {6, 5, 0x0e2},
    {5, 6, 0x11e},  {4, 7, 0x157},   {3, 7, 0x17c},   {2, 8, 0x1b0},   {1, 8, 0x1d7},   {0, 8, 0x200},
    {-1, 8, 0x229}, {-2, 8, 0x250},  {-3, 7, 0x284},  {-4, 7, 0x2a9},  {-5, 6, 0x2e2},  {-6, 5, 0x31e},
    {-7, 4, 0x357}, {-7, 3, 0x37c},  {-8, 2, 0x3b0},  {-8, 1, 0x3d7},  {-8, 0, 0x400},  {-8, -1, 0x429},
    {-8, -2, 0x450},{-7, -3, 0x484}, {-7, -4, 0x4a9}, {-6, -5, 0x4e2}, {-5, -6, 0x51e}, {-4, -7, 0x557},
    {-3, -7, 0x57c},{-2, -8, 0x5b0}, {-1, -8, 0x5d7}, {0, -8, 0x600},  {1, -8, 0x629},  {2, -8, 0x650},
    {3, -7, 0x684}, {4, -7, 0x6a9},  {5, -6, 0x6e2},  {6, -5, 0x71e},  {7, -4, 0x757},  {7, -3, 0x77c},
    {8, -2, 0x7b0}, {8, -1, 0x7d7},
};

/// The order the original tests the probes in: rows from the top down, right half first.
/// Only the *last* probe to report a hit decides the material, so the order is observable.
constexpr u8 kProbeOrder[44] = {35, 34, 33, 32, 31, 37, 36, 30, 29, 38, 28, 39, 27, 40, 26,
                                41, 25, 42, 24, 43, 23, 0,  22, 1,  21, 2,  20, 3,  19, 4,
                                18, 5,  17, 6,  16, 7,  8,  14, 15, 9,  10, 11, 12, 13};

}  // namespace

void Physics::init(const TableData& table, bool highResolution) {
  table_ = &table;
  highResolution_ = highResolution;
  layout_ = TableLayout::resolve(table);
  if (!layout_.complete()) throw DataError(table.name + ": the engine structures could not be located");
  const ByteView d = table.dataSegment;
  if (d.size() < layout_.sineTable + 2560 * 2) throw DataError("data segment too small for the sine table");

  // The sine table is stored big-endian and byte-swapped by the original at start-up.
  for (std::size_t i = 0; i < sine_.size(); ++i)
    sine_[i] = static_cast<i16>(rd16be(d, layout_.sineTable + i * 2));

  for (std::size_t i = 0; i < materials_.size(); ++i) {
    const std::size_t o = layout_.materials + i * 16;
    materials_[i] = {static_cast<i16>(rd16le(d, o)),     static_cast<i16>(rd16le(d, o + 2)),
                     static_cast<i16>(rd16le(d, o + 4)), static_cast<i16>(rd16le(d, o + 6)),
                     static_cast<i16>(rd16le(d, o + 8))};
  }

  // Each table defines its own number of ramp zones, and Stones 'n Bones has far more than
  // the others because of its rails.
  static constexpr int kRampCounts[4] = {4, 6, 5, 11};
  const int ramps = kRampCounts[std::clamp(table.index, 0, 3)];
  const std::size_t g = highResolution ? layout_.gravityHighRes : layout_.gravityLowRes;
  ramps_.clear();
  for (int i = 0; i < ramps; ++i) {
    const std::size_t at = g + static_cast<std::size_t>(i) * 4;
    if (at + 4 > d.size()) break;
    ramps_.push_back({static_cast<i16>(rd16le(d, at)), static_cast<i16>(rd16le(d, at + 2))});
  }
  if (ramps_.empty()) ramps_.push_back({0, 10});
  currentGravity_ = ramps_[0];

  limits_.velocityMin = static_cast<i16>(rd16le(d, layout_.limits));
  limits_.velocityMax = static_cast<i16>(rd16le(d, layout_.limits + 2));
  limits_.bumperKick = static_cast<i16>(rd16le(d, layout_.limits + 4));
  limits_.slingshotKick = static_cast<i16>(rd16le(d, layout_.limits + 6));
  limits_.slingshotMinimum = static_cast<i16>(rd16le(d, layout_.limits + 8));
  limits_.nudgeLift = static_cast<i16>(rd16le(d, layout_.limits + 10));
  limits_.nudgeReturn = static_cast<i16>(rd16le(d, layout_.limits + 12));

  if (layout_.layerZonesWhenUpper) zonesWhenUpper_ = TableLayout::readZoneList(d, layout_.layerZonesWhenUpper);
  if (layout_.layerZonesWhenLower) zonesWhenLower_ = TableLayout::readZoneList(d, layout_.layerZonesWhenLower);

  walls_ = table.walls;
  rampWalls_ = table.ramps;
  dynamic_ = table.dynamic;
  zoneLower_ = &table.occlusionA;
  rubberUpper_ = &table.occlusionB;
  zoneUpper_ = &table.occlusionC;

  // Each record's frame block is named by the instruction that fills it in at run time,
  // so the frames are reshaped straight from that block using the record's own geometry.
  for (std::size_t i = 0; i < flippers_.size(); ++i) {
    Flipper& f = flippers_[i];
    f = Flipper::decode(d, layout_.flipperRecords + i * 0x3c);
    f.maskSegment = layout_.flipperSegments[i];
    if (!f.valid() || f.maskSegment == 0) continue;
    const int pitch = f.frameWidth / 8;
    const int rows = (f.maxFrame + 1) * f.frameRows;
    if (pitch <= 0 || rows <= 0) continue;
    ByteView block;
    try {
      block = table.image.segment(f.maskSegment);
    } catch (const DataError&) {
      continue;
    }
    if (block.size() < static_cast<std::size_t>(rows) * pitch) continue;
    f.frameStack = static_cast<int>(i);
    flipperMasks_[i] = Mask::fromPacked(block, pitch * 8, rows, pitch);
  }

  // Every flipper frame carries the surrounding static wall pixels, so stamping a frame
  // into the playfield mask never erases the walls around the flipper.
  for (Flipper& f : flippers_) {
    if (!f.valid()) continue;
    if (f.frameStack < 0) continue;
    Mask& stack = flipperMasks_[static_cast<std::size_t>(f.frameStack)];
    const int pitch = f.frameWidth / 8;
    const int frames = f.maxFrame + 1;
    for (int fr = 0; fr < frames; ++fr) {
      for (int row = 0; row < f.frameRows; ++row) {
        const int srcRow = fr * f.frameRows + row;
        if (srcRow >= stack.height()) break;
        const int wallRow = f.originY + row;
        if (wallRow < 0 || wallRow >= walls_.height()) continue;
        for (int b = 0; b < pitch && b < stack.pitch(); ++b) {
          const int wallByte = f.originX / 8 + b;
          if (wallByte < 0 || wallByte >= walls_.pitch()) continue;
          stack.packedMutable()[static_cast<std::size_t>(srcRow) * stack.pitch() + b] |=
              table.walls.rowBytes(wallRow)[wallByte];
        }
      }
    }
  }
}

int Physics::clampVelocity(int v) const {
  return std::clamp(v, static_cast<int>(limits_.velocityMin), static_cast<int>(limits_.velocityMax));
}

void Physics::addTilt(int amount) {
  tiltCounter_ += amount;
  if (tiltCounter_ > 120) {
    tilted_ = true;
    flippersEnabled_ = false;
  }
}

void Physics::decayTilt() {
  if (tiltCounter_ > 0) --tiltCounter_;
}

int Physics::takeBumper() {
  const int b = firedBumper_;
  firedBumper_ = -1;
  return b;
}

void Physics::selectGravity(Ball& ball) {
  const Mask& solidA = ball.upper ? *rubberUpper_ : dynamic_;
  const Mask& solidB = ball.upper ? rampWalls_ : walls_;
  const Mask& zones = ball.upper ? *zoneUpper_ : *zoneLower_;
  const int byteColumn = (ball.x + 8) >> 3;
  const int row = ball.y + 8;
  if (row < 0 || row >= solidB.height()) return;
  for (int i = -1; i <= 1; ++i) {
    const int bc = byteColumn + i;
    if (bc < 0 || bc >= solidB.pitch()) continue;
    const u8 combined = static_cast<u8>(solidA.rowBytes(row)[bc] | solidB.rowBytes(row)[bc]);
    if (combined != 0) continue;
    const int zone = zones.rowBytes(row)[bc] & 0x0f;
    if (zone < static_cast<int>(ramps_.size())) currentGravity_ = ramps_[static_cast<std::size_t>(zone)];
    return;
  }
}

void Physics::setPatch(const PhysmapPatch& patch, bool raised) {
  Mask& target = patch.overhead ? rampWalls_ : walls_;
  target.patch(patch.byteX, patch.y, patch.byteWidth, patch.height, raised ? patch.raised : patch.dropped);
}

/// Ramps are not modelled with a height; instead the ball moves between two sets of masks
/// when it passes through the zones that mark a ramp entrance or exit.
void Physics::updateLayer(Ball& ball) const {
  const int cx = ball.x + 8;
  const int cy = ball.y + 8 + nudgeLiftPixels_;
  const auto& zones = ball.upper ? zonesWhenUpper_ : zonesWhenLower_;
  for (const Rect& r : zones) {
    if (r.contains(cx, cy)) {
      ball.upper = !ball.upper;
      return;
    }
  }
}

void Physics::frameStart(Ball& ball, const Controls& controls) {
  subStep(ball, controls);
  subStep(ball, controls);
  decayTilt();
  selectGravity(ball);
  updateLayer(ball);
}

void Physics::frameEnd(Ball& ball, const Controls& controls) {
  subStep(ball, controls);
  if (!highResolution_) subStep(ball, controls);
}

Contact Physics::probe(const Ball& ball) const {
  const Mask& wall = ball.upper ? rampWalls_ : walls_;
  const int cx = ball.centreX();
  const int cy = ball.centreY() + nudgeLiftPixels_;
  int sumAngle = 0, count = 0, downHalf = 0, quadrants = 0, last = -1;
  for (u8 index : kProbeOrder) {
    const ProbePoint& p = kProbes[index];
    if (!wall.get(cx + p.dx, cy + p.dy)) continue;
    sumAngle += p.angle;
    ++count;
    if (p.angle <= 0x400) ++downHalf;  // the point due left counts with the quarter below it
    quadrants |= p.angle == 0x400 ? 2 : 1 << (p.angle >> 9);
    last = index;
  }
  if (count == 0 || last < 0) return {};
  // Hits spanning angle zero must be unwrapped before the mean is taken.
  if (quadrants == 0x9 || quadrants == 0xb || quadrants == 0xd) sumAngle += downHalf * 0x800;

  Contact c;
  c.hit = true;
  c.count = count;
  c.angle = (sumAngle / count) & 0x7ff;

  const int mx = cx + kProbes[last].dx;
  const int my = cy + kProbes[last].dy;
  if (my >= TableData::kHeight) return {};
  if (ball.upper) {
    if (rubberUpper_->get(mx, my)) c.materialClass |= 1;
    if (rampWalls_.get(mx, my)) c.materialClass |= 2;
    if (zoneUpper_->get(mx, my)) c.materialClass |= 4;
  } else {
    if (dynamic_.get(mx, my)) c.materialClass |= 1;
    if (walls_.get(mx, my)) c.materialClass |= 2;
    if (zoneLower_->get(mx, my)) c.materialClass |= 4;
  }

  const int ci = ((c.angle * 44 + 1024) >> 11) % 44;
  c.pointX = ball.x + kProbes[ci].dx + 8;
  c.pointY = ball.y + kProbes[ci].dy + 8;
  return c;
}

int Physics::flipperImpulse(const Ball&, int px, int py, int& fx, int& fy) const {
  fx = 0;
  fy = 0;
  for (const Flipper& f : flippers_) {
    if (!f.valid() || !f.box.contains(px, py)) continue;
    int dx = px - f.pivotX;
    int dy = py - f.pivotY;
    if (f.keySide == 2) {
      if (dx < 0) continue;
    } else {
      if (dx >= 0) continue;
      if (!f.vertical) {
        dx = -dx;
        dy = -dy;
      }
    }
    int a;
    if (f.vertical) {
      std::swap(dx, dy);
      a = std::abs(dy) >> 1;
    } else {
      a = std::abs(dy) >> 2;
    }
    fy = -f.omega * (dx + a);
    fx = f.omega * dy;
    return 1;
  }
  return 0;
}

void Physics::bounce(Ball& ball, const Contact& c) {
  if (onBounce) onBounce(ball, c);
  const Material& m = materials_[static_cast<std::size_t>(c.materialClass) & 7];
  const int vx = clampVelocity(ball.vx + flipperVx_);
  const int vy = clampVelocity(ball.vy + flipperVy_ + tableVelocity_);
  const i32 cosA = cos(c.angle);
  const i32 sinA = sin(c.angle);

  // Rotate into the surface frame. The shifts leave the result at twice the real value,
  // which the reverse rotation undoes.
  i32 vn = ((static_cast<i32>(vx) * cosA + static_cast<i32>(vy) * sinA) << 3) >> 16;
  i32 vt = ((-static_cast<i32>(vx) * sinA + static_cast<i32>(vy) * cosA) << 3) >> 16;
  if (vn <= 0) {  // moving away from the surface
    pendingBumper_ = -1;
    return;
  }
  vn = -vn;

  if (vn >= m.minNormalSpeed) {
    vn = 0;
    pendingBumper_ = -1;
  } else if (std::abs(16 * vt / vn) >= m.glanceLimit) {
    vn = 0;
    pendingBumper_ = -1;
  } else if (pendingBumper_ >= 0) {
    // A bumper always throws the ball back; a kicker needs a firm enough hit first.
    if (c.materialClass != 3) {
      vn += limits_.bumperKick;
      firedBumper_ = pendingBumper_;
    } else if (vn > limits_.slingshotMinimum) {
      pendingBumper_ = -1;
    } else {
      vn += limits_.slingshotKick;
      firedBumper_ = pendingBumper_;
    }
  }

  if (m.restitution != 0) vn -= vn * 256 / m.restitution;
  // The two gains are scaled by the softness of the hit in 16-bit registers (cs:0x8f95), so
  // steel's, at 10000 and 2500, wrap round for all but the hardest hits, and are then taken
  // as signed divisors. A gain of zero leaves the dividend's low word as the "quotient".
  i16 f0 = m.tangentGain, f1 = m.spinGain;
  if (vn >= -1023) {
    const u16 k = static_cast<u16>(((-vn) >> 6) + 1);
    f0 = static_cast<i16>(static_cast<u16>(f0) * k);
    f1 = static_cast<i16>(static_cast<u16>(f1) * k);
  }
  const i32 d = static_cast<i16>(ball.spin + tableVelocity_ - vt) * 256;
  vt += f0 != 0 ? static_cast<i16>(d / f0) : static_cast<i16>(d);
  ball.spin = static_cast<i16>(ball.spin - (f1 != 0 ? static_cast<i16>(d / f1) : static_cast<i16>(d)));
  vt = vt * 0x800 / 0x801;

  int nvx = static_cast<int>((cosA * vn - sinA * vt) >> 15);
  int nvy = static_cast<int>((sinA * vn + cosA * vt) >> 15);
  nvx -= flipperVx_;
  nvy -= flipperVy_ + tableVelocity_;
  ball.vx = static_cast<i16>(clampVelocity(nvx));
  ball.vy = static_cast<i16>(clampVelocity(nvy));

  // A deeply embedded ball is nudged back out along the normal by a quarter pixel.
  if (c.count >= 6) {
    ball.xFixed += (-1024 * cosA) >> 16;
    ball.yFixed += (-1024 * sinA) >> 16;
  }
  pendingBumper_ = -1;
}

void Physics::updateNudgeAndFlippers(Ball&, const Controls& controls) {
  if (controls.nudge && nudgeAccumulator_ < 0x800) {
    tableVelocity_ = limits_.nudgeLift;
    nudgeAccumulator_ = std::min(nudgeAccumulator_ + limits_.nudgeLift, 0x800);
  } else if (!controls.nudge && nudgeAccumulator_ > 0) {
    tableVelocity_ = limits_.nudgeReturn;
    nudgeAccumulator_ = std::max(nudgeAccumulator_ + limits_.nudgeReturn, 0);
  } else {
    tableVelocity_ = 0;
  }
  nudgeLiftPixels_ = nudgeAccumulator_ >> 9;

  for (Flipper& f : flippers_) {
    if (!f.valid()) continue;
    const bool held = f.keySide == 2 ? controls.leftFlipper : controls.rightFlipper;
    f.update(held, flippersEnabled_ && !tilted_);
  }
}

void Physics::stampFlippers(const Ball& ball) {
  if (!table_) return;
  for (Flipper& f : flippers_) {
    if (!f.valid() || !f.box.contains(ball.x, ball.y)) continue;
    if (f.frame == f.lastStamped) continue;
    if (f.frameStack < 0) continue;
    const Mask& stack = flipperMasks_[static_cast<std::size_t>(f.frameStack)];
    const int pitch = f.frameWidth / 8;
    const std::size_t offset = static_cast<std::size_t>(f.frame) * f.frameRows * stack.pitch();
    if (offset >= stack.packed().size()) continue;
    walls_.patch(f.originX / 8, f.originY, std::min(pitch, stack.pitch()), f.frameRows,
                 stack.packed().subspan(offset));
    f.lastStamped = f.frame;
  }
}

void Physics::integrate(Ball& ball) const {
  ball.yFixed += ball.vy;
  ball.y = static_cast<i16>(ball.yFixed / 1024);
  if (ball.y >= TableData::kHeight) ball.lost = true;
  ball.xFixed += ball.vx;
  ball.x = static_cast<i16>(ball.xFixed / 1024);
  ball.vy = static_cast<i16>(ball.vy + currentGravity_.y);
  ball.vx = static_cast<i16>(ball.vx + currentGravity_.x);
  if (ball.spin > 0) ball.spin = static_cast<i16>(std::max(0, ball.spin - 2));
  else if (ball.spin < 0) ball.spin = static_cast<i16>(std::min(0, ball.spin + 2));
}

void Physics::subStep(Ball& ball, const Controls& controls) {
  if (!ball.active) {
    updateNudgeAndFlippers(ball, controls);
    stampFlippers(ball);
    return;
  }
  const Contact c = probe(ball);
  if (c.hit) {
    touched_ = true;
    contact_ = {c.pointX, c.pointY};
    pendingBumper_ = -1;
    // Rubber means a kicker, plastic means a bumper; each has its own list.
    if (!tilted_ && bumpers_ && (c.materialClass == 3 || c.materialClass == 7)) {
      const bool wantKicker = c.materialClass == 3;
      for (std::size_t i = 0; i < bumpers_->size(); ++i) {
        const Bumper& b = (*bumpers_)[i];
        if (b.kicker == wantKicker && b.rect.contains(c.pointX, c.pointY)) {
          pendingBumper_ = static_cast<int>(i);
          break;
        }
      }
    }
    flipperVx_ = flipperVy_ = 0;
    if (c.materialClass == 2) flipperImpulse(ball, c.pointX, c.pointY, flipperVx_, flipperVy_);
    bounce(ball, c);
  }
  updateNudgeAndFlippers(ball, controls);
  integrate(ball);
  stampFlippers(ball);
}

void Physics::launch(Ball& ball, int pull, u32 randomValue) const {
  const int perUnit = highResolution_ ? -138 : -118;
  ball.vx = 0;
  ball.vy = static_cast<i16>(clampVelocity(perUnit * pull - static_cast<int>(randomValue & 0xff)));
  ball.spin = static_cast<i16>(randomValue & 0x0f);
}

}  // namespace encore
