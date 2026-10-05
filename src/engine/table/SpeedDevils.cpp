#include "engine/table/SpeedDevils.h"

namespace encore {

SpeedDevils::SpeedDevils(ByteView prg) : Flow(prg, 1) {
  bindRules();
  bindSteps();
  bindNative(0x0004, [this] { lightsOut(); });
  bindNative(0x0008, [this] {  // a new game
    lightsOut();
    stopBlinks();
    at0217();
    clearBall();
    for (u16 player = 0; player < 8; ++player) savePlayer(static_cast<u16>(player * 0x5c));
    b(0x0071) = 0;
    b(0x0070) = 0;
    b(0x10ae) = 0;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(0x1d41 + i)) = 0x20;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(0x1d53 + i)) = 0x20;
  });
  bindNative(0x0149, [this] {  // the music while nobody plays
    music(0x0a47);
    b(0x211b) = b(0x0a49);
    b(0x2119) = 9;
  });
  bindNative(0x0162, [this] {  // the music a game opens with
    music(0x0a41);
    b(0x211b) = b(0x0a43);
    b(0x2119) = 0;
  });
  bindNative(0x0175, [this] {  // the ball is lost
    b(0x3396) = 0xff;
    w(0x334d) = high() ? 0x103 : 0x171;
    placeBall(0x118, 0x230);
    b(0x2d3a) = 0xff;
    b(0x3397) = 0;
    b(0x33aa) = 0;
    b(0x3351) = 0;
    award(0x063a);
    addTimer(0x01f1);
  });
  bindNative(0x01f1, [this] {
    if (!countTo(0x3661, 5)) return;
    sfx(0x09fe);
    endTimer();
  });
  bindNative(0x0c70, [this] {
    w(0x378d) = 0;
    if (b(0x33a8) == 0) endTimer();
  });
  bindNative(0x1078, [this] {  // tilt
    b(0x3397) = 0;
    b(0x2119) = 0x3e;
    music(0x0a4d);
    startScript(0x1d1b);
    stopBlinks();
    lightsOut();
  });
  bindNative(0x1097, [this] { music(0x0a4a); });
  bindNative(0x109e, [this] {  // a flipper pressed: each row of three lane lights moves along one
    if (b(0x3353) != 0xff) return;
    b(0x3353) = 0;
    auto rotate = [this](u16 flags) {
      const u8 first = b(flags);
      b(flags) = b(static_cast<u16>(flags + 1));
      b(static_cast<u16>(flags + 1)) = b(static_cast<u16>(flags + 2));
      b(static_cast<u16>(flags + 2)) = first;
    };
    rotate(0x362d);
    row(0x362d, 3, 0x10);
    b(0x1067) = b(0x1068) = b(0x1069) = 0;
    rotate(0x3630);
    row(0x3630, 3, 0x13);
    b(0x106a) = b(0x106b) = b(0x106c) = 0;
    if (w(0x1073) != 0) return;
    w(0x106f) = 0;
    w(0x106d) = 0;
    w(0x1071) = 0;
    rotate(0x3623);
    row(0x3623, 3, 0x06);
  });
  bindNative(0x241a, [this] { everyFrame(); });
  bindNative(0x2512, [this] {  // after a bumper's score
    at24de();
    w(0x378d) = 0;
    if (w(0x378f) == 0xff) {
      startScript(0x17fa);
      w(0x378f) = 0;
    }
  });
  bindNative(0x2711, [this] {  // each second of a count-down: at nought the mode ends
    if (b(0x378a) != 0x2a || b(0x378b) != 0x37) return;
    for (u16 mode : {u16{0x10a8}, u16{0x10a7}})
      if (b(mode) == 0xff) {
        b(0x211c) = 1;
        music(mode == 0x10a8 ? 0x0aa1 : 0x0aaa);
        b(0x2119) = 3;
      }
    b(0x10a7) = 0;
    b(0x10a8) = 0;
    b(0x3351) = 0;
    b(0x2119) = 3;
    b(0x211c) = 1;
  });
}

void SpeedDevils::row(u16 flags, u16 count, u8 firstLight) {
  for (u16 i = 0; i < count; ++i) {
    const u8 light = static_cast<u8>(firstLight + i);
    stopBlink(light);
    if (b(static_cast<u16>(flags + i)) == 0xff) lightOn(light);
    else lightOff(light);
  }
}

void SpeedDevils::lights(std::initializer_list<u8> which, void (SpeedDevils::*what)(u8)) {
  for (u8 light : which) (this->*what)(light);
}

void SpeedDevils::scored() {
  b(0x33a6) = 0xff;
  w(0x378d) = 0;
  if (w(0x378f) == 0xff) {
    startScript(0x17fa);
    w(0x378f) = 0;
  }
}

void SpeedDevils::addBonus(u16 amount) {
  u16 times = w(0x3379);
  do addScore(0x3361, amount);
  while (times-- > 1);
}

void SpeedDevils::hold() {
  b(0x2d3a) = 0xff;
  placeBall(0x100, 0x29);
}

void SpeedDevils::sameBallAgain() {
  --b(0x0071);
  w(0x378d) = 0;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x4671 + i)) = 0x12;
  w(0x467d) = 0;
  bx = 0x157e;
  call(nativeW(bx));
}

void SpeedDevils::at0217() {
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x096a + i)) = b(static_cast<u16>(0x0982 + i));
}

void SpeedDevils::clearBall() {
  b(0x2047) = 0x38;
  b(0x2062) = 0x38;
  b(0x2058) = 0x38;
  for (u16 at : {u16{0x464d}, u16{0x3361}, u16{0x1373}, u16{0x1424}})
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(at + i)) = 0;
  b(0x007a) = 0;
  w(0x10a1) = 0;
  for (u16 a = 0x1067; a <= 0x106c; ++a) b(a) = 0;
  for (u16 a : {u16{0x106d}, u16{0x106f}, u16{0x1071}, u16{0x1089}, u16{0x1073}}) w(a) = 0;
  b(0x1077) = 0;
  b(0x1078) = 0;
  for (u16 a : {u16{0x107f}, u16{0x107d}, u16{0x1083}, u16{0x1081}, u16{0x1085}}) w(a) = 0;
  b(0x331e) = 0;
  b(0x10aa) = 0;
  b(0x10ab) = 0;
  b(0x095d) = 1;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x0058 + i)) = 0;
  w(0x0056) = 0;
  w(0x10a5) = 0x1e;
  w(0x10a3) = 0x28;
  for (u16 a : {u16{0x108b}, u16{0x1087}, u16{0x107b}, u16{0x1079}}) w(a) = 0;
  b(0x33aa) = 0;
  b(0x10a8) = 0;
  b(0x10a7) = 0;
}

namespace {
struct Kept {
  u16 inRecord, at, bytes;
};
/// What a player's record holds, and where each piece lives while that player plays
/// (cs:0f9e). The last two are only ever brought back, not kept (cs:0cbc).
constexpr Kept kKept[] = {
    {0xb3, 0x464d, 12}, {0xbf, 0x3361, 12}, {0xcb, 0x0058, 12}, {0xd7, 0x0056, 2}, {0xdd, 0x10a5, 2}, {0xdf, 0x10a3, 2},
    {0xd9, 0x108b, 2},  {0xdb, 0x1087, 2},  {0xe3, 0x107b, 2},  {0xe1, 0x1079, 2}, {0xe5, 0x3626, 1}, {0xe6, 0x3633, 4},
    {0xea, 0x3627, 5},  {0xef, 0x364f, 5},  {0xf6, 0x10ab, 1},  {0xf4, 0x10a1, 2},
};
constexpr Kept kBroughtBackOnly[] = {{0x103, 0x1373, 12}, {0xf7, 0x1424, 12}};
}  // namespace

void SpeedDevils::savePlayer(u16 player) {
  for (const Kept& k : kKept)
    for (u16 i = 0; i < k.bytes; ++i) b(static_cast<u16>(player + k.inRecord + i)) = b(static_cast<u16>(k.at + i));
}

void SpeedDevils::restorePlayer() {
  const u16 player = static_cast<u16>((b(0x37b1) - 1) * 0x5c);
  for (const Kept& k : kKept)
    for (u16 i = 0; i < k.bytes; ++i) b(static_cast<u16>(k.at + i)) = b(static_cast<u16>(player + k.inRecord + i));
  for (const Kept& k : kBroughtBackOnly)
    for (u16 i = 0; i < k.bytes; ++i) b(static_cast<u16>(k.at + i)) = b(static_cast<u16>(player + k.inRecord + i));
  // and the lights are put as that says
  if (b(0x3626) != 0) {
    blink(0x09, later(b(0x007a), 0x0f), 0x0f);
    lightOn(0x09);
  }
  for (u16 i = 0; i < w(0x1087); ++i) setLight(static_cast<u8>(0x1a + i));
  row(0x3633, 4, 0x16);
  for (u16 i = 0; i < 5; ++i) {
    const u8 light = static_cast<u8>(0x32 + i);
    if (b(static_cast<u16>(0x364f + i)) != 0xff) continue;
    blink(light, (light & 1) ? later(b(0x007a), 0x0f) : b(0x007a), 0x0f);
  }
  for (u16 i = 0; i < 5; ++i)
    if (b(static_cast<u16>(0x3627 + i)) == 0xff) blink(static_cast<u8>(0x0a + i), 0, 0x0f);
  if (b(0x3626) == 0xff) lightOn(0x09);
  for (u16 i = 0; i < w(0x1079); ++i) b(static_cast<u16>(0x363d + i)) = 0xff;
  row(0x363d, 10, 0x20);
  for (u16 n = static_cast<u16>(w(0x107b) - w(0x1079)), light = static_cast<u16>(w(0x1079) + 0x20); n > 0; --n, ++light)
    blink(static_cast<u8>(light), (light & 1) ? later(b(0x007a), 0x0f) : b(0x007a), 0x0f);
  if (u16 n = w(0x108b); n != 0) {
    if (n >= 0x0c) n = 0x0c;
    for (u16 i = 0; i < n; ++i) b(static_cast<u16>(0x3655 + i)) = 0xff;
    row(0x3655, 12, 0x38);
  }
}

bool SpeedDevils::leftLanes() {
  at24de();
  addScore(0x464d, 0x09b2);
  scored();
  addBonus(0x09ca);
  sfx(0x0a2a);
  if (b(0x362d) != 0xff || b(0x362e) != 0xff || b(0x362f) != 0xff) return false;
  at0225();
  return true;
}

bool SpeedDevils::rightLanes() {
  at24de();
  addScore(0x464d, 0x09be);
  scored();
  addBonus(0x09d6);
  sfx(0x0a2a);
  if (b(0x3630) != 0xff || b(0x3631) != 0xff || b(0x3632) != 0xff) return false;
  at0225();
  return true;
}

bool SpeedDevils::at1638() {
  for (u16 a = 0x3633; a <= 0x3636; ++a)
    if (b(a) != 0xff) return false;
  at17ad();
  at16f0();
  for (u16 a = 0x3633; a <= 0x3636; ++a) b(a) = 0;
  for (u8 light = 0x16; light <= 0x19; ++light) blink(light, 0, 2);
  addTimer(0x16b6);
  award(0x05ae);
  return true;
}

void SpeedDevils::at16f0() {
  const u16 n = w(0x1087);
  if (n == 5) {
    w(0x1089) = 0x1e;
    for (u8 light = 0x1a; light <= 0x1f; ++light) blink(light, 0, 1);
    return;
  }
  ++w(0x1087);
  setLight(static_cast<u8>(n + 0x1a));
}

void SpeedDevils::at1748() {
  w(0x1087) = 0;
  for (u8 light = 0x1a; light <= 0x1f; ++light) {
    stopBlink(light);
    clearLight(light);
  }
  if (b(0x361f) == 0xff) return;
  blink(0x02, b(0x007a), 0x0f);
  b(0x10ab) = 0;
  b(0x361f) = 0xff;
}

void SpeedDevils::at17ad() {
  const u16 n = w(0x107b);
  if (n == 0x0a) {
    award(0x0493);
    return;
  }
  blink(static_cast<u8>(n + 0x20), b(0x007a), 0x0f);
  blink(static_cast<u8>(n + 0x21), later(b(0x007a), 0x0f), 0x0f);
  w(0x107b) += 2;
}

void SpeedDevils::at17f1() {
  const u16 n = w(0x1079);
  if (w(0x107b) <= n) return;
  ++w(0x1079);
  stopBlink(static_cast<u8>(n + 0x20));
  setLight(static_cast<u8>(n + 0x20));
  award(0x05e6);
  if (w(0x1079) != 0x0a) return;
  blink(0x09, later(b(0x007a), 0x0f), 0x0f);
  setLight(0x09);
  blink(0x0f, b(0x007a), 0x0f);
  setLight(0x0f);
  w(0x1075) = 0x4b0;
  award(0x0602);
}

void SpeedDevils::at186a() {
  for (u8 light = 0x29; light >= 0x20; --light) clearLight(light);
  w(0x1079) = 0;
  w(0x107b) = 0;
  if (b(0x33aa) == 0xff) addTimer(0x18be);
  else at18db();
}

void SpeedDevils::at18db() {
  award(0x061e);
  b(0x33aa) = 0xff;
  b(0x10a8) = 0xff;
}

void SpeedDevils::at18ed() {
  if (b(0x33aa) == 0xff) addTimer(0x1903);
  else at1920();
}

void SpeedDevils::at1920() {
  b(0x3351) = 0;
  music(0x0aa7);
  b(0x33aa) = 0xff;
  startScript(0x12bc);
  b(0x10a7) = 0xff;
}

void SpeedDevils::at1b4f() {
  for (u16 light = 0x32; light < 0x37; ++light)
    if (b(static_cast<u16>(0x361d + light)) != 0xff) return;
  for (u8 light = 0x32; light <= 0x36; ++light) clearLight(light);
  for (u8 light = 0x32; light <= 0x36; ++light) stopBlink(light);
  for (u8 light = 0x32; light <= 0x36; ++light) blink(light, 0, 1);
  addTimer(0x1bd9);
  w(0x10a1) = 0;
}

bool SpeedDevils::at2031() {
  at24de();
  sfx(0x0a2a);
  award(0x0690);
  for (u16 light = 6; light < 9; ++light)
    if (b(static_cast<u16>(0x361d + light)) != 0xff) return false;
  if (w(0x1073) != 0) return false;
  award(0x06ad);
  if (static_cast<u8>(b(0x1078) + b(0x1077)) >= 8) {
    award(0x0704);
  } else if (++b(0x1077) == 1) {
    b(0x3621) = 0xff;
    blink(0x04, 0, 0x0a);
  }
  for (u8 light = 6; light <= 8; ++light) blink(light, 0, 2);
  w(0x1073) = 0x28;
  return true;
}

void SpeedDevils::at219b() {
  if (w(0x1083) != 0xff || w(0x1081) != 0xff || w(0x1085) != 0xff) return;
  award(0x05ca);
  w(0x1083) = 0;
  w(0x1081) = 0;
  w(0x1085) = 0;
  const u8 light = static_cast<u8>(w(0x108b) + 0x38);
  if (light & 1) at2201();
  ++w(0x108b);
  if (light < 0x43) setLight(light);
  else lightOn(0x43);
}

void SpeedDevils::at2201() {
  if (w(0x10a1) == 5) return;
  const u16 n = w(0x10a1);
  ++w(0x10a1);
  const u8 light = b(static_cast<u16>(0x108d + n * 4 + 2));
  blink(light, b(0x007a), 0x0f);
  setLight(light);
}

void SpeedDevils::at226b() {
  at17f1();
  award(0x0704);
  w(0x1083) = 0xff;
  w(0x1081) = 0xff;
}

void SpeedDevils::threeDigits(u16 value, u16 at, bool blankHundreds) {
  for (int i = 2; i >= 0; --i) {
    u16 digit = value % 10;
    value /= 10;
    if (blankHundreds && digit == 0 && i == 0) digit = 0xfff3;
    b(static_cast<u16>(at + i)) = static_cast<u8>(digit + 0x37);
  }
}

/// cs:2281: a lap. The laps are counted, with awards at ten, twenty and at the two counts
/// that move up twenty at a time, and the display's lines about them are made up.
void SpeedDevils::at2281() {
  at0225();
  award(static_cast<u16>(0x0721 + (w(0x108b) > 0x0b ? 0x0b : w(0x108b)) * 0x1d));
  if (b(0x10a8) == 0xff) {
    award(0x0393);
    at24f8();
  }
  do {
    ++w(0x0056);
    addScore(0x0058, 0x098e);
  } while (w(0x0056) == 1);
  const u16 laps = w(0x0056);
  if (laps <= 0x14) {
    if (laps > 0x0a) {
      award(0x03e9);
      if (laps == 0x14) {
        blink(0x01, 0, 0x0f);
        setLight(0x01);
        award(0x04cb);
      }
    }
    if (laps == 0x0a) at18ed();
    else if (laps < 0x0a) award(0x03cc);
  }
  if (laps == w(0x10a5)) {
    w(0x10a5) += 0x14;
    if (b(0x3622) != 0xff) {
      setLight(0x05);
      blink(0x05, 0, 0x0f);
      award(0x0506);
    }
  } else if (static_cast<u16>(w(0x10a5) - laps) < 0x0a) {
    threeDigits(w(0x10a5), 0x1924, true);
    award(0x03af);
  }
  if (static_cast<u16>(w(0x10a3) - w(0x0056)) <= 0x0a) {
    if (w(0x0056) == w(0x10a3)) {
      w(0x10a3) += 0x14;
      at18ed();
    }
    threeDigits(w(0x10a3), 0x1937, true);
    award(0x0406);
  }
  threeDigits(w(0x0056), 0x19b5, false);
  for (u16 at = 0x19b5; b(at) == 0x37; ++at) b(at) = 0x2a;  // no noughts in front
}

void SpeedDevils::at24de() {
  if (b(0x10a7) != 0xff) return;
  addScore(0x1373, 0x09e2);
  b(0x33a7) = 0xff;
}

void SpeedDevils::at24f8() {
  if (b(0x10a8) != 0xff) return;
  addScore(0x1424, 0x09a6);
  b(0x33a7) = 0xff;
}

void SpeedDevils::everyFrame() {
  if (++b(0x007a) == 0x1e) b(0x007a) = 0;
  struct Count { u16 at; void (SpeedDevils::*then)(); };
  auto lane = [this](u8 light) {
    stopBlink(light);
    lightOn(light);
    row(0x3623, 3, 0x06);
  };
  auto down = [this](u16 at) { return w(at) != 0 && --w(at) == 0; };
  if (down(0x106d)) lane(0x08);
  if (down(0x106f)) lane(0x07);
  if (down(0x1071)) lane(0x06);
  if (down(0x1073)) {  // cs:20d7
    for (u8 light = 6; light <= 8; ++light) stopBlink(light);
    for (u8 light = 6; light <= 8; ++light) clearLight(light);
    row(0x3623, 3, 0x06);
  }
  down(0x107d);
  down(0x107f);
  if (down(0x1089)) at1748();
  if (down(0x1075)) {  // cs:185f
    clearLight(0x0f);
    stopBlink(0x0f);
  }
}

void SpeedDevils::bindRules() {
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bindNative(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };

  bindNative(0x11d1, [this] {  // out of the lane on to the table
    w(0x2cfe) = 0;
    b(0x331e) = 0;
  });

  // the two rows of three lanes
  struct Lane { u16 roll, busy, lit; u8 light; u16 timer, counter; bool left; };
  for (const Lane l : {Lane{0x11de, 0x1067, 0x362d, 0x10, 0x120b, 0x366b, true}, {0x1236, 0x1068, 0x362e, 0x11, 0x1263, 0x366d, true},
                       {0x128e, 0x1069, 0x362f, 0x12, 0x12bb, 0x366f, true}, {0x12e6, 0x106a, 0x3630, 0x13, 0x1313, 0x3671, false},
                       {0x133e, 0x106b, 0x3631, 0x14, 0x136b, 0x3673, false}, {0x1396, 0x106c, 0x3632, 0x15, 0x13c3, 0x3675, false}}) {
    bindNative(l.roll, [this, l] {
      if (b(l.busy) == 0xff) return;
      b(l.busy) = 0xff;
      b(l.lit) = 0xff;
      if (l.left ? leftLanes() : rightLanes()) {  // all three: they flash, and a light of the four above comes on
        const u8 first = l.left ? 0x10 : 0x13;
        const u16 busy = l.left ? 0x1067 : 0x106a;
        for (u16 i = 0; i < 3; ++i) b(static_cast<u16>(busy + i)) = 0xff;
        for (u8 i = 0; i < 3; ++i) blink(static_cast<u8>(first + i), 0, 1);
        b(l.left ? 0x3635 : 0x3636) = 0xff;
        addTimer(l.left ? 0x1436 : 0x14d9);
        if (!at1638()) blink(l.left ? 0x18 : 0x19, 0, 1);
        return;
      }
      blink(l.light, 0, 1);
      addTimer(l.timer);
    });
    after(l.timer, l.counter, 0x0a, [this, l] {
      stopBlink(l.light);
      if (b(l.busy) == 0xff) {
        lightOn(l.light);
        b(l.busy) = 0;
      }
      endTimer();
    });
  }
  struct Rows { u16 timer, counter; u8 light, first; u16 lit, busy; };
  for (const Rows r : {Rows{0x1436, 0x3677, 0x18, 0x10, 0x362d, 0x1067}, {0x14d9, 0x3679, 0x19, 0x13, 0x3630, 0x106a}}) {
    after(r.timer, r.counter, 0x28, [this, r] {
      stopBlink(r.light);
      lightOn(r.light);
      for (u16 i = 0; i < 3; ++i) b(static_cast<u16>(r.lit + i)) = 0;
      for (u8 i = 0; i < 3; ++i) stopBlink(static_cast<u8>(r.first + i));
      for (u8 i = 0; i < 3; ++i) lightOff(static_cast<u8>(r.first + i));
      for (u16 i = 0; i < 3; ++i) b(static_cast<u16>(r.busy + i)) = 0;
      endTimer();
    });
  }
  after(0x16b6, 0x367b, 0x2d, [this] {
    for (u8 light = 0x16; light <= 0x19; ++light) stopBlink(light);
    for (u8 light : {u8{0x18}, u8{0x19}, u8{0x16}, u8{0x17}}) lightOff(light);
    endTimer();
  });
  bindNative(0x18be, [this] {
    if (b(0x3396) != 0xff) {
      if (b(0x33aa) == 0xff) return;
      at18db();
    }
    endTimer();
  });
  bindNative(0x1903, [this] {
    if (b(0x3396) != 0xff) {
      if (b(0x33aa) == 0xff) return;
      at1920();
    }
    endTimer();
  });

  bindNative(0x193e, [this] {  // the pit
    bool pitStop = false;
    if (b(0x3620) == 0xff) {
      stopBlink(0x03);
      clearLight(0x03);
      if (b(0x3626) == 0xff) {
        award(0x045b);
        pitStop = true;
      } else {
        b(0x33bf) = 0xff;
        b(0x3351) = 0;
        awardAlways(0x0423);
      }
    }
    if (!pitStop) {
      if (b(0x3626) == 0xff) {
        stopBlink(0x09);
        clearLight(0x09);
        at186a();
      }
      if (b(0x361f) == 0xff) {
        award(0x0477);
        stopBlink(0x02);
        clearLight(0x02);
        b(0x10aa) = 0xff;
      }
    }
    if (b(0x10a9) != 0xff) addTimer(pitStop ? 0x1a29 : 0x19d1);
  });
  bindNative(0x19d1, [this] {
    hold();
    if (!countTo(0x367d, 0x14)) return;
    addTimer(0x1a81);
    endTimer();
  });
  bindNative(0x1a29, [this] {
    hold();
    if (!countTo(0x367f, 0x96)) return;
    addTimer(0x1a81);
    endTimer();
  });
  bindNative(0x1a81, [this] {
    hold();
    if (b(0x33aa) != 0xff && !countTo(0x3681, 0x3c)) return;
    sfx(0x0a0e);
    hold();
    b(0x2d3a) = 0;
    w(0x2cfe) = 0x320;
    w(0x2cfc) = 0xf7cc;
    addTimer(0x1b38);
    endTimer();
  });
  after(0x1b38, 0x3683, 0x3c, [this] {
    b(0x10a9) = 0;
    endTimer();
  });
  after(0x1bd9, 0x3685, 0x78, [this] {
    for (u8 light = 0x32; light <= 0x36; ++light) stopBlink(light);
    endTimer();
  });

  // a gear's light taken: its place in the row of five blinks
  auto gear = [this](u16 lit, u8 light, u16 record, u16 flag, u8 rowLight, bool outOfStep) {
    if (b(lit) != 0xff) return;
    clearLight(light);
    stopBlink(light);
    award(record);
    b(flag) = 0xff;
    blink(rowLight, outOfStep ? later(b(0x007a), 0x0f) : b(0x007a), 0x0f);
    at1b4f();
  };
  bindNative(0x1c04, [=, this] {
    if (b(0x10a8) == 0xff) {
      award(0x0393);
      at24f8();
    }
    award(0x06e6);
    gear(0x3628, 0x0b, 0x0522, 0x364f, 0x32, true);
    gear(0x3629, 0x0c, 0x0576, 0x3650, 0x33, false);
    if (b(0x3621) == 0xff) {  // cs:1cd0
      ++b(0x2047);
      ++b(0x1078);
      ++b(0x095d);
      if (--b(0x1077) == 0) {
        clearLight(0x04);
        stopBlink(0x04);
      }
      u16 n = 0, record = 0x087d;
      while (n < 7 && b(static_cast<u16>(0x3647 + n)) != 0) {
        ++n;
        record = static_cast<u16>(record + 0x1c);
      }
      award(record);
      b(static_cast<u16>(0x3647 + n)) = 0xff;
      lightOn(static_cast<u8>(n + 0x2a));
    }
    b(0x3634) = 0xff;
    at0225();
    if (at1638()) return;
    blink(0x17, 0, 1);
    addTimer(0x1cb4);
  });
  after(0x1cb4, 0x3687, 0x0a, [this] {
    stopBlink(0x17);
    lightOn(0x17);
    endTimer();
  });
  bindNative(0x1d1f, [=, this] {
    if (w(0x331c) == 0x1ec5) {
      gear(0x362b, 0x0e, 0x055a, 0x3651, 0x34, true);
      gear(0x3627, 0x0a, 0x0592, 0x3653, 0x36, false);
      if (b(0x361e) == 0xff) {
        award(0x04af);
        clearLight(0x01);
        stopBlink(0x01);
        setLight(0x37);
        ++b(0x0071);
      }
      return;
    }
    if (w(0x331c) != 0x1ec6) return;
    if (b(0x10a8) == 0xff) {
      award(0x0393);
      at24f8();
    }
    at0225();
    if (b(0x362c) == 0xff) {
      addScore(0x464d, 0x096a);
      at0217();
      if (b(0x10a8) == 0xff) {
        b(0x3351) = 0;
        awardAlways(0x043f);
        b(0x33bf) = 0xff;
      } else {
        award(0x043f);
      }
      clearLight(0x0f);
      stopBlink(0x0f);
      setLight(0x03);
      blink(0x03, b(0x007a), 0x0f);
      addTimer(0x1ea9);
    }
    if (b(0x3622) == 0xff) {
      award(0x04ea);
      clearLight(0x05);
      stopBlink(0x05);
    }
    gear(0x362a, 0x0d, 0x053e, 0x3652, 0x35, false);
    w(0x1085) = 0xff;
    at219b();
    b(0x3633) = 0xff;
    at0225();
    if (at1638()) return;
    blink(0x16, 0, 1);
    addTimer(0x1ec7);
  });
  after(0x1ea9, 0x3689, 0x4b0, [this] {
    stopBlink(0x03);
    clearLight(0x03);
    endTimer();
  });
  bindNative(0x1ec5, [] {});
  bindNative(0x1ec6, [] {});
  after(0x1ec7, 0x368b, 0x0a, [this] {
    stopBlink(0x16);
    lightOn(0x16);
    endTimer();
  });

  // the three lanes at the bottom
  struct Bottom { u16 roll, wait, lit; u8 light; bool lightFirst; };
  for (const Bottom l : {Bottom{0x1ee3, 0x106d, 0x3625, 0x08, false}, {0x1f4f, 0x106f, 0x3624, 0x07, true}, {0x1fc0, 0x1071, 0x3623, 0x06, true}}) {
    bindNative(l.roll, [this, l] {
      if (w(l.wait) != 0) return;
      if (l.lightFirst) lightOn(l.light);
      b(l.lit) = 0xff;
      if (at2031()) return;
      w(l.wait) = 0x14;
      blink(l.light, 0, 1);
    });
  }
  bindNative(0x212f, [this] { award(0x0657); });
  bindNative(0x2136, [this] {
    sfx(0x0a1a);
    award(0x0673);
  });
  bindNative(0x2151, [this] { award(0x06c9); });
  bindNative(0x2158, [this] {  // the ball came back down the lane: it is given again
    if (w(0x331c) != 0x11d1) return;
    b(0x33ab) = 0;
    music(0x0a44);
    b(0x2119) = 1;
    startScript(0x12a4);
    b(0x33a8) = 0;
    b(0x10ad) = 0;
    b(0x3352) = 0;
  });
  bindNative(0x218d, [this] { b(0x3352) = 0; });
  bindNative(0x2194, [this] { b(0x3352) = 0xff; });
  bindNative(0x2231, [this] {
    w(0x107d) = 0x186;
    if (w(0x107f) != 0) {
      w(0x107f) = 0;
      at226b();
    }
    at2281();
  });
  bindNative(0x224e, [this] {
    w(0x107f) = 0x186;
    if (w(0x107d) != 0) {
      w(0x107d) = 0;
      at226b();
    }
    at2281();
  });
}

/// The steps Speed Devils adds to the display's scripts.
void SpeedDevils::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait1 = [this] {
    w(0x33b1) = 1;
    w(0x33af) = 0x4a84;
  };
  bindNative(0x0602, [this] {
    stopBlink(0x09);
    clearLight(0x09);
    at186a();
    goTo(0x138f);
  });
  bindNative(0x0618, [this] {
    if (b(0x33bf) != 0xff) return nextStep(4);
    b(0x33bf) = 0;
    bx = 0x137f;
    w(0x33ad) = bx;
    b(0x10a8) = 0xff;
    call(nativeW(bx));
  });
  bindNative(0x06f6, [=, this] {
    wait1();
    b(0x33aa) = 0;
    b(0x3351) = 0;
    nextStep(4);
  });
  bindNative(0x0711, [this] {  // the match begins (and its pace is set a first time)
    nativeCW(0x07e2) = high() ? 0x0d : 0x0b;
    call(F(0x07e5));
  });
  bindNative(0x2535, [=, this] {
    b(0x1d92) = static_cast<u8>(b(0x37b1) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(0x2550, [=, this] {
    b(0x1da9) = static_cast<u8>(b(0x37b1) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(0x256b, [this] { nextStep(4); });  // (the original rings the PC's bell here)
  bindNative(0x4acb, [] {});
  bindNative(0x2578, [this] {
    b(0x10ad) = 0xff;
    w(0x33b1) = 1;
    w(0x33af) = 0x4acb;
    nextStep(4);
  });
  bindNative(0x258c, [=, this] {
    wait1();
    nextStep(4);
  });
  bindNative(0x259b, [this] {
    w(0x33af) = 0x25aa;
    w(0x33b1) = 0x2c;
    nextStep(4);
  });
  bindNative(0x25aa, [this] { si = b(0x33ab) == 0xff ? 0x22 : 0; });
  bindNative(0x25bd, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < 12; ++i)
      if (nativeB(static_cast<u16>(arg(2) + i)) != 0) nought = false;
    bx = nought ? arg(4) : static_cast<u16>(bx + 6);
    if (nativeW(bx) == 0) {
      w(0x33ad) = 0;
      return;
    }
    w(0x33ad) = bx;
    call(nativeW(bx));
  });
  bindNative(0x2602, [=, this] {  // the bonus times its multiplier
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x095e + i)) = b(static_cast<u16>(0x3361 + i));
    for (u8 n = static_cast<u8>(b(0x095d) - 1); n > 0; --n) addScore(0x3361, 0x095e);
    b(0x1dd1) = static_cast<u8>(b(0x095d) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(0x2644, [=, this] {
    if (b(0x095d) != 1) {
      wait1();
      nextStep(4);
      return;
    }
    goTo(arg(2));
  });
  bindNative(0x2668, [=, this] { goTo(arg(2)); });
  bindNative(0x2674, [=, this] {
    wait1();
    addScore(0x3361, 0x1373);
    nextStep(2);
  });
  bindNative(0x269e, [=, this] {
    wait1();
    addScore(0x3361, 0x1424);
    nextStep(2);
  });
  bindNative(0x26c8, [=, this] {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x0064 + i)) = 0;
    wait1();
    u32 times = w(0x0056);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(0x0064, 0x099a);
    addScore(0x3361, 0x0064);
    nextStep(2);
  });
  bindNative(0x277e, [=, this] {
    wait1();
    setLight(0x37);
    ++b(0x0071);
    nextStep(4);
  });
}

}  // namespace encore
