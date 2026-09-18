#include "data/TableSound.h"

#include <algorithm>

namespace pfr {
namespace {

struct SoundAddresses {
  u16 flipper, drained, issueBall, spring, rollInner, tickBonus, gameStart, raiseTargets;
  u16 silence, plunger, main, attract, warnTilt, tilt, gameOverSad, gameOverHighScore;
  u16 drainedJingle, gameStartJingle, matchStart, matchWin;
};
constexpr SoundAddresses kTables[4] = {
    {0x0c2d, 0x0c31, 0x0c35, 0x0c3d, 0x0c49, 0x0c61, 0x0c65, 0x0c1d,
     0x0c6c, 0x0c6f, 0x0c72, 0x0c75, 0x0c78, 0x0c7b, 0x0c7e, 0x0c81, 0x0c84, 0x0c87, 0x0c90, 0x0c93},
    {0x09fa, 0x09fe, 0x0a02, 0x0a0a, 0x0a1a, 0x0a36, 0x0a12, 0x09f2,
     0x0a3e, 0x0a41, 0x0a44, 0x0a47, 0x0a4a, 0x0a4d, 0x0a50, 0x0a53, 0x0a56, 0x0a41, 0x0a59, 0x0a5c},
    {0x086d, 0x0871, 0x0875, 0x087d, 0x0889, 0x08a5, 0x08b9, 0x0865,
     0x08cf, 0x08de, 0x08d2, 0x08e1, 0x08b0, 0x08b3, 0x08d5, 0x08d8, 0x08b6, 0x08de, 0x08c3, 0x08c6},
    {0x08b8, 0x08bc, 0x08c0, 0x08c8, 0x08d4, 0x08e4, 0x08e8, 0x08b4,
     0x08ec, 0x08f2, 0x08f5, 0x08f8, 0x08fb, 0x08fe, 0x0901, 0x0904, 0x0907, 0x08ef, 0x090a, 0x090d},
};

Jingle readJingle(ByteView d, std::size_t at) {
  if (at + 3 > d.size()) return {};
  return {d[at], d[at + 1], d[at + 2]};
}

}  // namespace

namespace {
bool plausible(const Sfx& s) {
  return s.sample >= 1 && s.sample <= 31 && s.note >= 1 && s.note <= 36 && s.channel <= 3;
}
}  // namespace

Sfx readSfx(ByteView d, std::size_t at) {
  if (at + 4 > d.size()) return {};
  // The third byte is always zero in a well-formed record.
  if (d[at + 2] != 0) return {};
  return {d[at], d[at + 1], d[at + 3]};
}

/// The recorded addresses are not always exact for every copy of the game, so accept a
/// record found just either side of the expected one.
Sfx readSfxNear(ByteView d, std::size_t at) {
  if (const Sfx exact = readSfx(d, at); plausible(exact)) return exact;
  for (int delta : {-1, 1, -2, 2}) {
    if (delta < 0 && at < static_cast<std::size_t>(-delta)) continue;
    const Sfx s = readSfx(d, at + static_cast<std::size_t>(delta));
    if (plausible(s)) return s;
  }
  return {};
}

TableSounds extractSounds(ByteView d, int tableIndex) {
  const SoundAddresses& a = kTables[std::clamp(tableIndex, 0, 3)];
  TableSounds s;
  s.flipper = readSfxNear(d, a.flipper);
  s.drained = readSfxNear(d, a.drained);
  s.issueBall = readSfxNear(d, a.issueBall);
  s.spring = readSfxNear(d, a.spring);
  s.rollInner = readSfxNear(d, a.rollInner);
  s.tickBonus = readSfxNear(d, a.tickBonus);
  s.gameStart = readSfxNear(d, a.gameStart);
  s.raiseTargets = readSfxNear(d, a.raiseTargets);
  s.silence = readJingle(d, a.silence);
  s.plunger = readJingle(d, a.plunger);
  s.main = readJingle(d, a.main);
  s.attract = readJingle(d, a.attract);
  s.warnTilt = readJingle(d, a.warnTilt);
  s.tilt = readJingle(d, a.tilt);
  s.gameOverSad = readJingle(d, a.gameOverSad);
  s.gameOverHighScore = readJingle(d, a.gameOverHighScore);
  s.drainedJingle = readJingle(d, a.drainedJingle);
  s.gameStartJingle = readJingle(d, a.gameStartJingle);
  s.matchStart = readJingle(d, a.matchStart);
  s.matchWin = readJingle(d, a.matchWin);
  return s;
}

}  // namespace pfr
