#include "engine/table/StonesNBones.h"

namespace encore {

StonesNBones::StonesNBones(ByteView prg) : Flow(prg, 3) {
  bindMatch();
  bindRules();
  bindSteps();
  bindNative(0x0004, [this] { lightsOut(); });
  bindNative(0x0008, [this] {  // a new game
    lightsOut();
    stopBlinks();
    copy12(0x0275, 0x028d);
    clearBall();
    for (u16 player = 0; player < 8; ++player) savePlayer(static_cast<u16>(player * 0x43));
    b(0x0226) = 0;
    b(0x0206) = 0;
    b(0x026e) = 0xff;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(0x23aa + i)) = 0x20;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(0x23bc + i)) = 0x20;
  });
  bindNative(0x01f8, [this] {  // the music while nobody plays
    music(0x08f8);
    b(0x2797) = b(0x08fa);
    b(0x2795) = 9;
  });
  bindNative(0x0211, [this] {  // the music a game opens with
    music(0x08ef);
    b(0x2797) = b(0x08f1);
    b(0x2795) = 0;
  });
  bindNative(0x0224, [this] {  // the ball is lost
    b(0x38e6) = 0xff;
    w(0x389d) = high() ? 0x103 : 0x171;
    placeBall(0x8c, 0xd2);
    b(0x33b6) = 0xff;
    b(0x38e7) = 0;
    addTimer(0x02aa);
    b(0x38fa) = 0;
    out(0x26);
    b(0x38a1) = 0;
    award(0x0880);
  });
  bindNative(0x02aa, [this] {
    if (!countTo(0x3b9a, 5)) return;
    sfx(0x08bc);
    endTimer();
  });
  bindNative(0x0ced, [this] {
    w(0x3cc6) = 0;
    if (b(0x38f8) == 0) endTimer();
  });
  bindNative(0x0f58, [this] {  // tilt
    b(0x38e7) = 0;
    music(0x08fe);
    b(0x2795) = 0x34;
    startScript(0x2368);
    stopBlinks();
    lightsDark();
  });
  bindNative(0x0f77, [this] { music(0x08fb); });
  bindNative(0x0f7e, [this] {  // a flipper pressed: each row of three lights moves along one
    if (b(0x38a3) != 0xff) return;
    b(0x38a3) = 0;
    auto rotate = [this](u16 flags) {
      const u8 first = b(flags);
      b(flags) = b(static_cast<u16>(flags + 1));
      b(static_cast<u16>(flags + 1)) = b(static_cast<u16>(flags + 2));
      b(static_cast<u16>(flags + 2)) = first;
    };
    if (b(0x020a) != 0xff) {
      rotate(0x3b6e);
      row(0x3b6e, 0x01);
    }
    if (b(0x020b) == 0xff) return;
    rotate(0x3b71);
    row(0x3b71, 0x04);
  });
  bindNative(0x3189, [this] { everyFrame(); });
  bindNative(0x3336, [this] {  // after a bumper's score
    at3542();
    w(0x3cc6) = 0;
    if (w(0x3cc8) == 0xff) {
      startScript(0x22fc);
      w(0x3cc8) = 0;
    }
  });
  bindNative(0x3461, [this] {  // each second of a count-down: at nought the mode ends
    if (b(0x3cc3) != 0x2a || b(0x3cc4) != 0x37) return;
    for (u16 mode : {u16{0x1270}, u16{0x126f}})
      if (b(mode) == 0xff) {
        b(0x2798) = 1;
        music(0x095b);
        b(0x2795) = 3;
      }
    b(0x126f) = 0;
    b(0x1270) = 0;
    b(0x38a1) = 0;
    b(0x2795) = 3;
    b(0x2798) = 1;
  });
}

void StonesNBones::lights(std::initializer_list<u8> which, void (StonesNBones::*what)(u8)) {
  for (u8 light : which) (this->*what)(light);
}

void StonesNBones::scored(u16 amount) {
  addScore(0x4ba4, amount);
  b(0x38f6) = 0xff;
  w(0x3cc6) = 0;
  if (w(0x3cc8) == 0xff) {
    startScript(0x22fc);
    w(0x3cc8) = 0;
  }
}

void StonesNBones::addBonus(u16 amount) {
  u16 times = w(0x38c9);
  do addScore(0x38b1, amount);
  while (times-- > 1);
}

void StonesNBones::row(u16 flags, u8 firstLight) {
  for (u16 i = 0; i < 3; ++i) {
    const u8 light = static_cast<u8>(firstLight + i);
    stopBlink(light);
    if (b(static_cast<u16>(flags + i)) == 0xff) lightOn(light);
    else lightOff(light);
  }
}

void StonesNBones::shape(u16 segment, u16 record, bool second) {
  const u16 width = w(static_cast<u16>(record + 6));
  copyShape(static_cast<u16>(segment + kLoadSegment), w(static_cast<u16>(record + 4)),
            static_cast<u16>(w(static_cast<u16>(record + (second ? 2 : 0))) + width), width, w(static_cast<u16>(record + 8)), static_cast<u16>(width * 3));
}

void StonesNBones::light7() {
  if (b(0x0210) == 0xff) return;
  stopBlink(0x07);
  blink(0x07, 0, 0x14);
  b(0x0210) = 0xff;
}

void StonesNBones::at3528() {
  if (b(0x1270) != 0xff) return;
  addScore(0x1e30, 0x01d2);
  b(0x38f7) = 0xff;
}

void StonesNBones::at3542() {
  if (b(0x126f) != 0xff) return;
  addScore(0x1d3b, 0x01c6);
  b(0x38f7) = 0xff;
}

void StonesNBones::clearBall() {
  b(0x26a3) = 0x38;
  b(0x26be) = 0x38;
  b(0x26b4) = 0x38;
  zero12(0x4ba4);
  zero12(0x38b1);
  zero12(0x01ee);
  copy12(0x1d3b, 0x00ee);
  copy12(0x1e30, 0x00ee);
  for (u16 i = 0; i < 11; ++i) b(static_cast<u16>(0x023c + i)) = 0;
  zero12(0x0212);
  copy12(0x022f, 0x00ee);
  b(0x386e) = 0;
  b(0x022e) = 0;
  b(0x0207) = 0;
  b(0x0208) = 0;
  b(0x020a) = 0xff;
  b(0x020b) = 0;
  w(0x020e) = 0;
  b(0x020c) = 0;
  b(0x020d) = 0;
  b(0x0210) = 0;
  shapeA(0x123d);
  shapeA(0x1265);
  copy12(0x0299, 0x02a5);
  copy12(0x02bd, 0x02c9);
  copy12(0x02e1, 0x02ed);
  w(0x01ea) = 0x0a;
  w(0x01ec) = 0;
  const u8 first = static_cast<u8>(chance(3, static_cast<u8>(w(0x3905)) % 3) + 1);  // one of the three lights, by chance
  b(0x0211) = first;
  blink(first, 0, 1);
  for (u16 a : {u16{0x021e}, u16{0x0220}, u16{0x0221}, u16{0x0222}, u16{0x0223}, u16{0x0224}, u16{0x0225}, u16{0x0227}, u16{0x0228},
                u16{0x021f}, u16{0x0229}, u16{0x022a}, u16{0x022b}, u16{0x022c}, u16{0x022d}, u16{0x022e}})
    b(a) = 0;
  b(0x023b) = 0x27;
  b(0x0247) = 0;
  w(0x0248) = 0;
  b(0x1270) = 0;
  b(0x126f) = 0;
  w(0x024b) = 0;
  w(0x024d) = 0;
  w(0x024f) = 0;
  b(0x0251) = 0;
  b(0x0252) = 0;
  b(0x0254) = 0;
  w(0x0255) = 0;
  w(0x0257) = 0;
  for (u16 a : {u16{0x0259}, u16{0x025a}, u16{0x025b}, u16{0x025c}, u16{0x025e}, u16{0x025f}, u16{0x0260}}) b(a) = 0;
  b(0x0261) = 1;
  b(0x0272) = 0;
  b(0x0273) = 0;
  b(0x0274) = 0;
}

namespace {
/// The lights a player keeps from ball to ball, and where in the player's record each is.
struct KeptLight {
  u8 light;
  u16 inRecord;
};
constexpr KeptLight kKeptLights[] = {{0x1a, 0xd2c}, {0x1b, 0xd2d}, {0x1c, 0xd2e}, {0x1d, 0xd2f}, {0x1e, 0xd30}, {0x14, 0xd31},
                                     {0x15, 0xd32}, {0x16, 0xd33}, {0x17, 0xd34}, {0x04, 0xd29}, {0x05, 0xd2a}, {0x06, 0xd2b}};
}  // namespace

void StonesNBones::savePlayer(u16 player) {
  copy12(static_cast<u16>(player + 0x0d02), 0x4ba4);
  copy12(static_cast<u16>(player + 0x0d0e), 0x38b1);
  copy12(static_cast<u16>(player + 0x0d1c), 0x0212);
  for (const KeptLight& k : kKeptLights) b(static_cast<u16>(player + k.inRecord)) = isLit(k.light) ? 0xff : 0;
  w(static_cast<u16>(player + 0x0d43)) = w(0x01ea);
  w(static_cast<u16>(player + 0x0d35)) = w(0x01ec);
  copy12(static_cast<u16>(player + 0x0d37), 0x01ee);
  b(static_cast<u16>(player + 0x0d28)) = b(0x021e);
  b(static_cast<u16>(player + 0x0d1a)) = b(0x020c);
  b(static_cast<u16>(player + 0x0d1b)) = b(0x020d);
}

void StonesNBones::restorePlayer() {
  const u16 player = static_cast<u16>((b(0x3cea) - 1) * 0x43);
  copy12(0x4ba4, static_cast<u16>(player + 0x0d02));
  copy12(0x38b1, static_cast<u16>(player + 0x0d0e));
  copy12(0x0212, static_cast<u16>(player + 0x0d1c));
  flushColours();
  int n = 0;
  for (const KeptLight& k : kKeptLights) {
    if (b(static_cast<u16>(player + k.inRecord)) == 0xff) setLight(k.light);
    else clearLight(k.light);
    if (++n == 5) flushColours();  // (the colours waiting to be set go out part way through)
  }
  flushColours();
  b(0x021e) = b(static_cast<u16>(player + 0x0d28));
  if (b(0x021e) == 0xff) {
    stopBlink(0x2c);
    blink(0x2c, 0, 0x12);
    shapeB(0x1265);
  }
  w(0x01ea) = w(static_cast<u16>(player + 0x0d43));
  w(0x01ec) = w(static_cast<u16>(player + 0x0d35));
  copy12(0x01ee, static_cast<u16>(player + 0x0d37));
  b(0x020c) = b(static_cast<u16>(player + 0x0d1a));
  for (u8 n2 = b(0x020c); n2 > 0; --n2) setLight(b(static_cast<u16>(0x0305 + (n2 - 1) * 7)));
  b(0x020d) = b(static_cast<u16>(player + 0x0d1b));
  if (b(0x020d) == 0xff) {
    blink(b(static_cast<u16>(0x0305 + b(0x020c) * 7)), 0, 0x20);
    stopBlink(0x10);
    blink(0x10, 0, 0x12);
  }
}

void StonesNBones::sameBallAgain() {
  --b(0x0226);
  w(0x3cc6) = 0;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x4bc8 + i)) = 0x12;
  w(0x4bd4) = 0;
  bx = 0x203e;
  call(nativeW(bx));
}

/// The match here counts a digit down, slower and slower... by a list of waits (ds:0000, or
/// ds:0048 at the faster screen mode), not by chance at every step.
void StonesNBones::bindMatch() {
  auto plain = [this] {
    setFont(3);
    w(0x4a69) = 0;
    w(0x4a6b) = 0;
  };
  bindNative(0x07f4, [=, this] {
    b(0x2798) = 1;
    music(0x0910);
    b(0x2795) = 0x34;
    w(0x38ff) = 0x08a4;
    for (u16 player = 0; player < b(0x3ce6); ++player) {
      const u16 digit = static_cast<u16>(0x2000 | static_cast<u8>(b(static_cast<u16>(0x0d02 + 0x0a + player * 0x43)) + 0x37));
      w(static_cast<u16>(0x23aa + player * 2)) = digit;
      w(static_cast<u16>(0x23bc + player * 2)) = digit;
    }
    plain();
    drawText(0x23aa, 0xa8);
    w(0x23d2) = chance(10, static_cast<u16>(w(0x3905) % 10));
    nativeCW(0x089f) = high() ? 0x48 : 0;
    si = 0x24;
    nativeCW(0x08a1) = nativeW(nativeCW(0x089f));
    w(0x3901) = si;
  });
  bindNative(0x08a4, [=, this] {
    u16 left = si;
    plain();
    drawText(0x23d0, static_cast<u16>(0x540 + (w(0x23d2) << 3)));  // the last digit rubbed out
    if (--nativeCW(0x08a1) == 0) {
      nativeCW(0x08a1) = nativeW(nativeCW(0x089f));
      nativeCW(0x089f) += 2;
      --left;
      if (--w(0x23d2) == 0xffff) w(0x23d2) = 9;
    }
    const u16 digit = w(0x23d2);
    b(0x23ce) = static_cast<u8>(digit + 0x37);
    plain();
    w(0x3ccc) = 0x23ce;
    w(0x3cce) = static_cast<u16>(0x540 + (digit << 3));
    w(0x3cca) = F(0x6ca5);
    si = left;
    if (left != 0) return;
    // the last one stands: who has it?
    nativeCB(0x08a3) = 0;
    for (u16 player = 0; player < b(0x3ce6); ++player) {
      if (b(static_cast<u16>(0x23aa + player * 2)) == static_cast<u8>(digit + 0x37)) nativeCB(0x08a3) = 0xff;
      else w(static_cast<u16>(0x23bc + player * 2)) = 0x2a2a;
    }
    if (nativeCB(0x08a3) != 0xff) return;
    w(0x38a8) = 3;
    w(0x38aa) = 3;
    w(0x38ac) = 0xff;
    b(0x38ae) = 0xff;
    plain();
    drawText(0x23bc, 0xa8);
    b(0x38a1) = 0;
    music(0x090d);
    b(0x2798) = 1;
    b(0x38a1) = 0;
    b(0x2795) = 0x34;
  });
}

void StonesNBones::at149a() {
  at3542();
  sfx(0x08dc);
  scored(0x01a2);
  addBonus(0x00fa);
}

void StonesNBones::at14f8() {
  at3542();
  sfx(0x08d8, 0x28);
  scored(0x0196);
  addBonus(0x0106);
}

/// cs:1556: all nine targets are down.
void StonesNBones::at1556() {
  at351c();
  b(0x0208) = 0xff;
  for (u16 i = 0; i < 11; ++i) b(static_cast<u16>(0x023c + i)) = 0;
  lights({0x14, 0x15, 0x16, 0x17, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e}, &StonesNBones::stop);
  if (b(0x020d) != 0xff) award(w(static_cast<u16>(0x030a + b(0x020c) * 7)));
  else award(0x0376);
  at1708();
  addTimer(0x15c7);
}

void StonesNBones::at1708() {
  if (b(0x020d) == 0xff) return;
  b(0x020d) = 0xff;
  blink(b(static_cast<u16>(0x0305 + b(0x020c) * 7)), 0, 0x20);
  blink(0x10, 0, 0x12);
}

void StonesNBones::at1795() {
  shapeA(0x1233);
  if (b(0x0222) != 0xff) return;
  addScore(0x022f, 0x01c6);
  addScore(0x4ba4, 0x022f);
  award(0x074c);
  out(0x13);
  b(0x0222) = 0;
}

/// cs:1af4: all three of the first row of lights.
void StonesNBones::at1af4() {
  at3504();
  at351c();
  b(0x0207) = 0xff;
  b(0x020a) = 0xff;
  b(0x0211) = 0;
  lights({0x01, 0x02, 0x03}, &StonesNBones::stop);
  lights({0x01, 0x02, 0x03}, &StonesNBones::flash);
  auto prize = [this](u16 flag, u8 light) {
    if (b(flag) == 0xff) return;
    blink(light, b(0x021f), 0x12);
    b(flag) = 0xff;
  };
  const u16 times = w(0x0248);
  if (times == 0) prize(0x0227, 0x0b);
  else if (times == 1 || times >= 4) prize(0x0228, 0x0c);
  else if (times == 2) prize(0x022d, 0x0e);
  else prize(0x022c, 0x0d);
  ++w(0x0248);
  addTimer(0x1c09);
  if (b(0x0210) != 0xff) {
    award(0x05a8);
    b(0x0210) = 0xff;
    stopBlink(0x07);
    blink(0x07, 0, 0x14);
  }
  shapeB(0x123d);
}

/// cs:2060: with none of the seven prizes waiting, the way in to them closes. True if one is.
bool StonesNBones::at2060() {
  for (u16 flag : {u16{0x0229}, u16{0x022b}, u16{0x022a}, u16{0x022c}, u16{0x022d}, u16{0x0228}, u16{0x0227}})
    if (b(flag) == 0xff) return true;
  b(0x0210) = 0;
  shapeA(0x123d);
  out(0x07);
  return false;
}

void StonesNBones::at2162() {
  sfx(0x08cc);
  b(0x33b6) = 0;
  place(0x8d, 0x8f, 0xff);
  w(0x337a) = 0xf2fb;
  w(0x3378) = 0;
  b(0x0274) = 0;
}

/// cs:1c68: the ball goes in where the prizes are given, each that is lit in its turn.
void StonesNBones::at1c68() {
  place(0x8d, 0x8f, 0xff);
  b(0x33b6) = 0xff;
  b(0x0274) = 0xff;
  at3528();
  at351c();
  b(0x390f) = 0xff;
  b(0x0271) = 0;
  if (b(0x38fa) == 0xff) {
    b(0x0271) = 0xff;
    b(0x026f) = b(0x1270);
  }
  b(0x0253) = b(0x0252);
  shapeA(0x123d);
  shapeB(0x1233);
  b(0x0210) = 0;
  out(0x07);
  b(0x024a) = 0;
  auto started = [this](bool script) { b(0x024a) = static_cast<u8>(b(0x024a) + (script ? 1 : 0)); };
  if (b(0x0252) == 0xff && b(0x0254) >= 1 && b(0x0254) <= 3) {
    const u8 step = b(0x0254);
    const bool script = award(step == 1 ? 0x048f : step == 2 ? 0x04ab : 0x04c7);
    if (b(0x2795) != 0x32) ++b(0x2795);
    started(script);
    b(0x38b0) = 0xff;
    b(0x0254) = step == 3 ? 0 : static_cast<u8>(step + 1);
    shapeB(0x123d);
    light7();
  }
  if (b(0x022b) == 0xff) {
    b(0x022b) = 0;
    out(0x0a);
    started(awardAlways(0x0730));
    b(0x38b0) = 0xff;
  }
  if (b(0x022a) == 0xff) {
    b(0x022a) = 0;
    out(0x09);
    started(awardAlways(0x0714));
    addScore(0x4ba4, 0x0275);
    copy12(0x0275, 0x028d);
    blink(0x0a, b(0x021f), 0x12);
    b(0x022b) = 0xff;
    if (b(0x0210) != 0xff) {
      b(0x0210) = 0xff;
      stopBlink(0x07);
      blink(0x07, 0, 0x14);
    }
    shapeB(0x123d);
    addTimer(0x202c);
    b(0x38b0) = 0xff;
  }
  if (b(0x0229) == 0xff) {
    b(0x0229) = 0;
    out(0x08);
    started(award(0x0688));
    ++b(0x0226);
    b(0x38b0) = 0xff;
  }
  if (b(0x022d) == 0xff) {
    b(0x022d) = 0;
    out(0x0e);
    started(award(0x06f8));
    addScore(0x38b1, 0x38b1);  // the bonus doubled
    b(0x38b0) = 0xff;
  }
  if (b(0x022c) == 0xff) {
    b(0x022c) = 0;
    out(0x0d);
    started(award(0x06dc));
    b(0x022e) = 0xff;
    b(0x38b0) = 0xff;
  }
  if (b(0x0228) == 0xff) {
    b(0x0228) = 0;
    out(0x0c);
    started(award(0x066c));
    b(0x38b0) = 0xff;
  }
  if (b(0x0227) == 0xff) {
    b(0x0227) = 0;
    out(0x0b);
    started(award(0x0634));
    b(0x38b0) = 0xff;
  } else {
    started(award(0x07a0));
    addScore(0x4ba4, 0x0299);
    copy12(0x0299, 0x02a5);
    if (b(0x0252) == 0xff) {
      shapeB(0x123d);
      stopBlink(0x07);
      blink(0x07, 0, 0x14);
      b(0x0210) = 0xff;
    }
  }
  if (b(0x024a) == 0) addTimer(0x211d);
  b(0x38b0) = 0;
}

void StonesNBones::at21c2() {
  if (b(0x3b86) == 0xff) {
    b(0x33b6) = 0xff;
    addTimer(0x2352);
    return;
  }
  b(0x33b6) = 0xff;
  placeBall(0x113, 0xf5);
  b(0x0273) = 0xff;
  at3528();
  at351c();
  b(0x024a) = 0;
  addScore(0x4ba4, 0x02e1);
  auto started = [this](bool script) { b(0x024a) = static_cast<u8>(b(0x024a) + (script ? 1 : 0)); };
  if (b(0x025b) == 0xff) {
    b(0x025c) = 0xff;
    b(0x38a1) = 0;
    const bool script = award(0x033d);
    b(0x38b0) = 0xff;
    started(script);
    placeBall(0x12c, 0x212);
    w(0x3378) = 0x0a;
    b(0x33b6) = 0;
    b(0x38b0) = 0xff;
    b(0x025d) = 0xff;
    b(0x2795) = 0;
    b(0x025b) = 0;
    stopBlink(0x19);
    setLight(0x19);
  }
  started(award(0x0784));
  if (b(0x0220) != 0) {
    b(0x0220) = 0;
    out(0x18);
    if (b(0x023b) <= 0x2b) {
      if (b(0x0261) == 1) ++b(0x0261);
      else b(0x0261) += 2;
      setLight(b(0x023b));
      started(award(static_cast<u16>(0x07bc + 0x1c * static_cast<u8>(b(0x023b) - 0x27))));
      ++b(0x023b);
    }
  }
  if (b(0x024a) == 0) addTimer(0x2352);
  b(0x38b0) = 0;
  b(0x26a3) = static_cast<u8>(b(0x0261) + 0x37);
}

void StonesNBones::at2506() {
  b(0x024a) = 0;
  if (b(0x0247) != 0xff) {
    out(0x2c);
    b(0x021e) = 0;
  }
  b(0x33b6) = 0xff;
  placeBall(2, 0x214);
  if (b(0x279c) == 0xff || b(0x3b7c) != 0) {
    addTimer(0x2919);
    return;
  }
  b(0x0272) = 0xff;
  at351c();
  auto started = [this](bool script) { b(0x024a) = static_cast<u8>(b(0x024a) + (script ? 1 : 0)); };
  if (b(0x025a) == 0xff) {
    b(0x025c) = 0xff;
    b(0x38a1) = 0;
    const bool script = award(0x033d);
    b(0x38b0) = 0xff;
    started(script);
    b(0x33b6) = 0;
    placeBall(0x12c, 0x212);
    w(0x3378) = 0x0a;
    b(0x38b0) = 0xff;
    b(0x025d) = 0xff;
    b(0x2795) = 0;
    b(0x025a) = 0;
    stopBlink(0x0f);
    setLight(0x0f);
  }
  if (b(0x020d) == 0xff) {  // the next of the eight is collected
    b(0x020d) = 0;
    out(0x10);
    const u16 record = static_cast<u16>(b(0x020c) * 7);
    const u8 light = b(static_cast<u16>(0x0305 + record));
    stopBlink(light);
    setLight(light);
    const u16 its = w(static_cast<u16>(0x0308 + record));
    if (b(0x38fa) == 0xff && its == 0x05e0) {
      addTimer(0x35b5);
    } else if (b(0x38fa) == 0xff && its == 0x05c4) {
      b(0x025f) = 0xff;
      addTimer(0x35da);
    } else {
      started(award(its));
    }
    call(w(static_cast<u16>(0x0306 + record)));
    if (++b(0x020c) >= 8) {
      b(0x020c) = 0;
      flushColours();
      b(0x0260) = 0xff;
      lights({0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}, &StonesNBones::flash);
      addTimer(0x2738);
    }
  }
  at3528();
  addScore(0x4ba4, 0x02bd);
  if (b(0x024a) != 0) b(0x38b0) = 0xff;
  const bool script = award(0x0768);
  b(0x38b0) = 0;
  started(script);
  if (b(0x024a) == 0) addTimer(0x2919);
}

void StonesNBones::at2a9c() {
  b(0x0209) = 0xff;
  b(0x020b) = 0xff;
  lights({0x04, 0x05, 0x06}, &StonesNBones::stop);
  lights({0x04, 0x05, 0x06}, &StonesNBones::flash);
  addTimer(0x2afb);
  if (b(0x021e) != 0xff) {
    b(0x021e) = 0xff;
    blink(0x2c, 0, 0x12);
    shapeB(0x1265);
  }
  award(0x056f);
}

void StonesNBones::digits(u16 value, u16 at) {
  for (int i = 2; i >= 0; --i) {
    b(static_cast<u16>(at + i)) = static_cast<u8>(value % 10 + 0x37);
    value /= 10;
  }
  // no noughts in front (and, as the original, none after either if they are all noughts)
  for (u16 n = 0; n < 0xffff && b(at) == 0x37; ++n, ++at) b(at) = 0x2a;
}

void StonesNBones::at2f85() {
  if (b(0x0229) == 0xff) return;
  b(0x0229) = 0xff;
  blink(0x08, b(0x021f), 0x12);
  shapeB(0x123d);
  b(0x0210) = 0xff;
  stopBlink(0x07);
  blink(0x07, 0, 0x14);
  award(0x0553);
}

void StonesNBones::at2fbe() {
  if (b(0x0228) == 0xff) return;
  b(0x0228) = 0xff;
  blink(0x0c, b(0x021f), 0x12);
  shapeB(0x123d);
  b(0x0210) = 0xff;
  stopBlink(0x07);
  blink(0x07, 0, 0x14);
  award(0x05a8);
}

/// cs:2d9e: a trip round the loop.
void StonesNBones::at2d9e() {
  sfx(0x08d0);
  scored(0x017e);
  addBonus(0x0136);
  if (b(0x025e) == 0xff) {
    out(0x12);
    w(0x0257) = 1;
    b(0x025e) = 0;
    const u16 both = static_cast<u16>(b(0x3b7c) + b(0x3b86));
    if (both == 0) award(0x0650);
    else if (both == 0xff) award(0x06a4);
    else if (both == 0x1fe) award(0x06c0);
    clearLight(0x0f);
    clearLight(0x19);
    b(0x38b0) = 0xff;
  }
  if (w(0x024f) == 0) b(0x0251) = 0;
  else if (b(0x0251) == 1) b(0x0251) = 2;
  else if (b(0x0251) != 2) b(0x0251) = 0;
  at3528();
  at3510();
  do {
    ++w(0x01ec);
    addScore(0x01ee, 0x089c);
    if (b(0x0221) != 0) {
      b(0x0221) = 0;
      out(0x11);
      addTimer(0x2f48);
    }
  } while (w(0x01ec) == 1);
  if (w(0x01ea) != w(0x01ec)) {
    const bool many = w(0x01ec) > 0x0a;
    digits(w(0x01ea), many ? 0x1ec5 : 0x1eb0);
    digits(w(0x01ec), 0x1e40);
    award(many ? 0x0864 : 0x0848);
    b(0x38b0) = 0;
    return;
  }
  if (w(0x01ec) == 0x0a) {
    at2f85();
  } else {
    at2fbe();
    digits(w(0x01ec), 0x1e40);
    award(0x0864);
  }
  w(0x01ea) += 0x0a;
  b(0x38b0) = 0;
}

void StonesNBones::everyFrame() {
  if (b(0x38e6) == 0xff || b(0x390f) == 0xff) return;
  if (w(0x020e) != 0) --w(0x020e);
  if (b(0x025c) != 0xff && w(0x0257) >= 1) {
    if (w(0x0257) == 1) {
      b(0x0259) = 0;
      if (b(0x025a) != 0) {
        b(0x025a) = 0;
        out(0x0f);
      }
      if (b(0x025b) != 0) {
        b(0x025b) = 0;
        out(0x19);
      }
    }
    --w(0x0257);
  }
  if (++b(0x0225) == 0x20) b(0x0225) = 0;
  if (++b(0x021f) == 0x24) b(0x021f) = 0;
  auto hurry = [this](u16 flag, u8 light) {
    if (b(flag) == 0) return;
    stopBlink(light);
    blink(light, 0, 1);
  };
  auto over = [this](u16 flag, u8 light) {
    if (b(flag) == 0) return;
    b(flag) = 0;
    out(light);
  };
  if (w(0x024d) != 0) {
    if (--w(0x024d) == 0) {
      over(0x0222, 0x13);
      over(0x0221, 0x11);
    }
    if (w(0x024d) == 0x5a) {
      hurry(0x0222, 0x13);
      hurry(0x0221, 0x11);
    }
  }
  if (w(0x024b) != 0) {
    if (--w(0x024b) == 0) over(0x0220, 0x18);
    if (w(0x024b) == 0x5a) hurry(0x0220, 0x18);
  }
  if (w(0x024f) != 0 && --w(0x024f) == 0) b(0x0251) = 0;
  if (w(0x0255) != 0 && --w(0x0255) == 0) {
    b(0x0252) = 0;
    b(0x0254) = 0;
    music(0x094f);
    b(0x2795) = b(0x08f5);
  }
}

void StonesNBones::bindRules() {
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bindNative(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };

  // the nine targets, in a row of four and a row of five
  struct Target { u16 hit, busy; u8 light; u16 timer, counter; bool five; };
  for (const Target t : {Target{0x103e, 0x023c, 0x14, 0x1082, 0x3ba2, false}, {0x10ba, 0x023d, 0x15, 0x10fe, 0x3ba4, false},
                         {0x1136, 0x023e, 0x16, 0x117a, 0x3ba6, false},       {0x11b2, 0x023f, 0x17, 0x11f6, 0x3ba8, false},
                         {0x122e, 0x0242, 0x1a, 0x1272, 0x3baa, true},        {0x12aa, 0x0243, 0x1b, 0x12ee, 0x3bac, true},
                         {0x1326, 0x0244, 0x1c, 0x136a, 0x3bae, true},        {0x13a2, 0x0245, 0x1d, 0x13e6, 0x3bb0, true},
                         {0x141e, 0x0246, 0x1e, 0x1462, 0x3bb2, true}}) {
    bindNative(t.hit, [this, t] {
      if (b(0x0260) == 0xff || b(0x0208) == 0xff || b(t.busy) == 0xff) return;
      b(t.busy) = 0xff;
      if (t.five) at14f8();
      else at149a();
      b(static_cast<u16>(0x3b6d + t.light)) = 0xff;
      for (u8 light : {u8{0x14}, u8{0x15}, u8{0x16}, u8{0x17}, u8{0x1a}, u8{0x1b}, u8{0x1c}, u8{0x1d}, u8{0x1e}})
        if (!isLit(light)) {
          addTimer(t.timer);
          blink(t.light, 0, 2);
          return;
        }
      at1556();
    });
    bindNative(t.timer, [this, t] {
      if (b(0x0208) != 0xff) {
        if (!countTo(t.counter, 0x0a)) return;
        stopBlink(t.light);
        setLight(t.light);
      } else {
        stopBlink(t.light);
      }
      b(t.busy) = 0;
      endTimer();
    });
  }
  after(0x15c7, 0x3bb4, 2, [this] {
    lights({0x14, 0x15, 0x16, 0x17, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e}, &StonesNBones::flash);
    addTimer(0x1630);
    endTimer();
  });
  after(0x1630, 0x3bb6, 0x46, [this] {
    lights({0x14, 0x15, 0x16, 0x17, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e}, &StonesNBones::stop);
    lights({0x14, 0x15, 0x16, 0x17, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e}, &StonesNBones::clear);
    b(0x0208) = 0;
    endTimer();
  });

  bindNative(0x1738, [this] {  // the ball came back down the lane: it is given again
    startScript(0x1c0f);
    b(0x386e) = 0;
    b(0x025d) = 0;
    b(0x38fb) = 0;
    b(0x38a1) = 0;
    music(0x08f5);
    b(0x2795) = 1;
    b(0x38f8) = 0;
  });
  bindNative(0x1768, [this] {
    at1795();
    b(0x025c) = 0;
    addScore(0x4ba4, 0x015a);
    addScore(0x38b1, 0x0112);
  });
  bindNative(0x1783, [] {});
  bindNative(0x1784, [this] {
    shapeB(0x1233);
    at3528();
  });
  bindNative(0x178e, [this] { shapeB(0x1233); });

  // the first row of three lights
  struct Lane { u16 roll; u8 light; u16 timer, counter; };
  for (const Lane l : {Lane{0x17cd, 0x01, 0x18af, 0x3bb8}, {0x18da, 0x02, 0x19bc, 0x3bba}, {0x19e7, 0x03, 0x1ac9, 0x3bbc}}) {
    bindNative(l.roll, [this, l] {
      if (b(0x0207) == 0xff) return;
      sfx(0x08d0);
      scored(0x017e);
      addBonus(0x011e);
      b(static_cast<u16>(0x3b6d + l.light)) = 0xff;
      if (b(0x0211) == l.light) {  // the one that was blinking
        stopBlink(l.light);
        addScore(0x0212, 0x01c6);
        addScore(0x4ba4, 0x0212);
        award(0x058c);
        at3504();
        at34f8();
        at351c();
        at3510();
      }
      if (b(0x0211) != 0) {
        b(0x020a) = 0;
        for (u8 other = 1; other <= 3; ++other)
          if (other != l.light) stopBlink(other);
        for (u8 other = 1; other <= 3; ++other)
          if (other != l.light) clearLight(other);
      }
      b(0x0211) = 0;
      if (isLit(0x01) && isLit(0x02) && isLit(0x03)) return at1af4();
      addTimer(l.timer);
      blink(l.light, 0, 2);
      b(0x020a) = 0xff;
    });
    bindNative(l.timer, [this, l] {
      if (b(0x0207) != 0xff) {
        if (!countTo(l.counter, 0x0a)) return;
        stopBlink(l.light);
        setLight(l.light);
        b(0x020a) = 0;
      }
      endTimer();
    });
  }
  after(0x1c09, 0x3bbe, 0x46, [this] {
    lights({0x01, 0x02, 0x03}, &StonesNBones::stop);
    lights({0x01, 0x02, 0x03}, &StonesNBones::clear);
    b(0x0207) = 0;
    b(0x020a) = 0;
    endTimer();
  });

  bindNative(0x1c68, [this] { at1c68(); });
  after(0x202c, 0x3bc0, 0x30c, [this] {
    if (b(0x022b) != 0) {
      out(0x0a);
      b(0x022b) = 0;
      shapeB(0x123d);
      at2060();
    }
    endTimer();
  });
  bindNative(0x20bf, [this] {
    place(0x8d, 0x8f, 0xff);
    b(0x33b6) = 0xff;
    addTimer(0x211d);
  });
  after(0x211d, 0x3bc2, 0x0a, [this] {
    b(0x390f) = 0;
    if (b(0x0271) == 0xff) {  // a mode was running: its count-down goes on
      u16 script = 0x1dd9;
      if (b(0x026f) != 0xff) {
        script = 0x1ca3;
        b(0x126f) = 0xff;
      } else {
        b(0x1270) = 0xff;
      }
      startScript(script);
    }
    at2162();
    endTimer();
  });
  bindNative(0x21bc, [this] {
    at2162();
    endTimer();
  });
  bindNative(0x21c2, [this] { at21c2(); });
  after(0x2352, 0x3bc4, 0x0a, [this] {
    sfx(0x08cc);
    b(0x33b6) = 0;
    placeBall(0x113, 0xf5);
    w(0x337a) = 0x682;
    w(0x3378) = 0xfd66;
    b(0x0273) = 0;
    endTimer();
  });
  bindNative(0x23bd, [this] {
    b(0x0273) = 0;
    addTimer(0x2352);
  });
  bindNative(0x23c9, [] {});
  for (u16 at : {u16{0x23ca}, u16{0x2425}})
    bindNative(at, [this] {
      sfx(0x08d4);
      scored(0x018a);
      addBonus(0x014e);
    });
  bindNative(0x2480, [this] {
    sfx(0x08d0);
    scored(0x01ae);
  });
  bindNative(0x24c3, [this] {
    sfx(0x08d0);
    scored(0x01ba);
  });
  bindNative(0x2506, [this] { at2506(); });
  after(0x2738, 0x3bc6, 0xf0, [this] {
    lights({0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}, &StonesNBones::stop);
    flushColours();
    lights({0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}, &StonesNBones::clear);
    flushColours();
    b(0x0260) = 0;
    endTimer();
  });

  // what each of the eight does when it is collected (the table at ds:0305)
  auto prize9 = [this] {
    if (b(0x022a) != 0xff) {
      b(0x022a) = 0xff;
      blink(0x09, b(0x021f), 0x12);
      shapeB(0x123d);
    }
    light7();
  };
  bindNative(0x27a5, [=, this] {
    b(0x2798) = 0;
    prize9();
  });
  bindNative(0x27e8, prize9);
  bindNative(0x2825, [] {});
  bindNative(0x2826, [this] {
    b(0x0259) = 0xff;
    w(0x0257) = 0x834;
    b(0x025a) = 0xff;
    b(0x025b) = 0xff;
    b(0x025c) = 0;
    stopBlink(0x0f);
    stopBlink(0x19);
    blink(0x0f, 0, 0x12);
    blink(0x19, 0, 0x12);
    b(0x025e) = 0xff;
    blink(0x12, 0, 0x12);
  });
  bindNative(0x286b, [this] {
    if (b(0x0229) == 0xff) return;
    b(0x0229) = 0xff;
    blink(0x08, b(0x021f), 0x12);
    shapeB(0x123d);
    b(0x0210) = 0xff;
    stopBlink(0x07);
    blink(0x07, 0, 0x14);
  });
  bindNative(0x289e, [this] {
    shapeB(0x123d);
    b(0x0254) = 1;
    b(0x0252) = 0xff;
    w(0x0255) = 0x960;
    b(0x2795) = 0x2e;
    light7();
  });

  after(0x2919, 0x3bc8, 0x0a, [this] {
    sfx(0x08cc);
    b(0x33b6) = 0;
    shapeB(0x1265);
    placeBall(2, 0x214);
    w(0x337a) = 0xf4c0;
    w(0x3378) = 0;
    b(0x0272) = 0;
    addTimer(0x2990);
    endTimer();
  });
  after(0x2990, 0x3bca, 0x1e, [this] {
    if (b(0x021e) != 0xff) shapeA(0x1265);
    b(0x0247) = 0;
    endTimer();
  });
  bindNative(0x29f6, [] {});
  bindNative(0x29f7, [this] { b(0x38a2) = 0xff; });
  bindNative(0x29fe, [this] { b(0x38a2) = 0; });
  bindNative(0x2a05, [this] {
    at3528();
    at3504();
    if (w(0x024f) != 0 && b(0x0251) == 2) award(0x0537);
    b(0x0251) = 0;
    const bool again = w(0x020e) != 0;
    w(0x020e) = 0x12c;
    if (again) {
      award(0x0618);
      return;
    }
    sfx(0x08d0);
    scored(0x0166);
    addBonus(0x012a);
  });
  after(0x2afb, 0x3bcc, 0x46, [this] {
    lights({0x04, 0x05, 0x06}, &StonesNBones::stop);
    lights({0x04, 0x05, 0x06}, &StonesNBones::clear);
    b(0x0209) = 0;
    b(0x020b) = 0;
    endTimer();
  });

  // the second row of three lights
  for (const Lane l : {Lane{0x2b5a, 0x04, 0x2be1, 0x3bce}, {0x2c19, 0x05, 0x2ca0, 0x3bd0}, {0x2cd8, 0x06, 0x2d5f, 0x3bd2}}) {
    bindNative(l.roll, [this, l] {
      if (b(0x0209) == 0xff) return;
      sfx(0x08d0);
      scored(0x018a);
      addBonus(0x014e);
      b(static_cast<u16>(0x3b6d + l.light)) = 0xff;
      if (isLit(0x04) && isLit(0x05) && isLit(0x06)) return at2a9c();
      addTimer(l.timer);
      blink(l.light, 0, 2);
      b(0x020b) = 0xff;
    });
    bindNative(l.timer, [this, l] {
      if (b(0x0209) != 0xff) {
        if (!countTo(l.counter, 0x14)) return;
        stopBlink(l.light);
        setLight(l.light);
      } else {
        stopBlink(l.light);
      }
      b(0x020b) = 0;
      endTimer();
    });
  }
  bindNative(0x2d97, [this] { w(0x020e) = 0x12c; });
  bindNative(0x2d9e, [this] { at2d9e(); });
  after(0x2f48, 0x3bd4, 2, [this] {
    at2d9e();
    endTimer();
  });
  bindNative(0x2ff7, [this] {
    if (b(0x279c) != 0xff) {
      at3528();
      at3504();
      sfx(0x08d0);
      scored(0x0166);
      addBonus(0x0142);
      auto light = [this](u16 flag, u8 which) {
        if (b(flag) == 0xff) return false;
        b(flag) = 0xff;
        stopBlink(which);
        blink(which, b(0x0225), 0x10);
        return true;
      };
      light(0x0222, 0x13);
      light(0x0221, 0x11);
      w(0x024d) = 0x1c2;
      if (b(0x023b) <= 0x2b && light(0x0220, 0x18)) w(0x024b) = 0x23a;
      b(0x0251) = 1;
      w(0x024f) = high() ? 0x3a8 : 0x30c;
    }
    rampB(0x1247);
  });
  bindNative(0x30f5, [this] {
    if (b(0x279c) != 0xff) {
      at3528();
      sfx(0x08d0);
      scored(0x0172);
      addBonus(0x011e);
    }
    rampA(0x1247);
  });
  bindNative(0x3164, [this] { rampB(0x1251); });
  bindNative(0x316b, [this] { rampA(0x1251); });
  bindNative(0x3172, [this] { rampB(0x125b); });
  bindNative(0x3179, [this] { rampA(0x125b); });
  bindNative(0x3180, [this] {
    b(0x0247) = 0xff;
    at3504();
  });
  for (u16 at : {u16{0x35b5}, u16{0x35da}})
    bindNative(at, [this, at] {  // an award that waits for the mode that is running to be over
      if (b(0x38fa) == 0xff) return;
      if (b(0x38e6) != 0xff) {
        b(0x025f) = 0xff;
        award(at == 0x35b5 ? 0x05e0 : 0x05c4);
      }
      endTimer();
    });
}

/// cs:36c0: sixteen rows of a tall picture (a bit a dot, every other bit) on the display.
void StonesNBones::scrollPicture(u16 row) {
  const u16 picture = 0x49ff + kLoadSegment;
  const u8 on = lit(), off = unlit();
  for (int plane = 0; plane < 2; ++plane) {
    const u8 first = plane == 0 ? 0x80 : 0x20, second = plane == 0 ? 0x08 : 0x02;
    u16 from = static_cast<u16>(row * 0x28), to = 0xa8;
    for (int r = 0; r < 0x10; ++r, to = static_cast<u16>(to + 0x58))
      for (int i = 0; i < 0x28; ++i) {
        const u8 bits = farB(picture, from++);
        dot(plane, to++) = (bits & first) ? on : off;
        dot(plane, to++) = (bits & second) ? on : off;
      }
  }
}

/// The steps Stones 'n Bones adds to the display's scripts.
void StonesNBones::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto simple = [this](u16 at, std::function<void()> what) {
    bindNative(at, [this, what] {
      wait1();
      what();
      nextStep(4);
    });
  };
  simple(0x075f, [this] {
    b(0x38fa) = 0;
    b(0x38a1) = 0;
  });
  simple(0x077a, [this] { b(0x38fa) = 0xff; });
  simple(0x078f, [this] { b(0x1270) = 0xff; });
  simple(0x07a3, [this] { b(0x1270) = 0; });
  simple(0x07b7, [this] { b(0x126f) = 0xff; });
  simple(0x07cb, [this] { b(0x126f) = 0; });
  bindNative(0x1fd6, [this] {
    const u16 step = bx;
    out(0x09);
    b(0x022a) = 0;
    out(0x0a);
    b(0x022b) = 0;
    if (b(0x0253) != 0xff && !at2060()) {
      b(0x0210) = 0;
      shapeA(0x123d);
      out(0x07);
    }
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(0x2106, [this] {
    const u16 step = bx;
    addTimer(0x211d);
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(0x233b, [this] {
    const u16 step = bx;
    addTimer(0x2352);
    wait1();
    bx = step;
    nextStep(4);
  });
  simple(0x28d8, [this] { out(0x26); });
  bindNative(0x28f3, [this] {
    const u16 step = bx;
    if (b(0x025f) != 0xff) addTimer(0x2919);
    b(0x025f) = 0;
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(0x29b7, [this] {  // the ball is let go from wherever it is held
    const u16 step = bx;
    if (b(0x0272) == 0xff) addTimer(0x2919);
    else if (b(0x0274) == 0xff) addTimer(0x21bc);
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(0x3359, [this] {
    w(0x38ff) = 0x3368;
    w(0x3901) = 0x2c;
    nextStep(4);
  });
  bindNative(0x3368, [this] { si = b(0x38fb) == 0xff ? 0x22 : 0; });
  bindNative(0x337b, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < 12; ++i)
      if (nativeB(static_cast<u16>(arg(2) + i)) != 0) nought = false;
    bx = nought ? arg(4) : static_cast<u16>(bx + 6);
    if (nativeW(bx) == 0) {
      w(0x38fd) = 0;
      return;
    }
    w(0x38fd) = bx;
    call(nativeW(bx));
  });
  bindNative(0x33bf, [=, this] {
    if (b(0x0261) == 1) return goTo(arg(2));
    wait1();
    nextStep(4);
  });
  bindNative(0x33e3, [=, this] { goTo(arg(2)); });
  bindNative(0x33ee, [this] {
    wait1();
    addScore(0x38b1, 0x1d3b);
    nextStep(2);
  });
  bindNative(0x3418, [this] {
    zero12(0x01fa);
    wait1();
    u32 times = w(0x01ec);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(0x01fa, 0x08a8);
    addScore(0x38b1, 0x01fa);
    nextStep(2);
  });
  simple(0x355c, [this] { b(0x1716) = static_cast<u8>(b(0x3cea) + 0x37); });
  simple(0x3577, [this] { b(0x2408) = static_cast<u8>(b(0x3cea) + 0x37); });
  bindNative(0x5a78, [] {});
  bindNative(0x3592, [this] {
    b(0x025d) = 0xff;
    w(0x3901) = 1;
    w(0x38ff) = 0x5a78;
    nextStep(4);
  });
  simple(0x35a6, [] {});
  bindNative(0x35ff, [this] {  // the bonus times its multiplier
    copy12(0x0262, 0x38b1);
    for (u8 n = static_cast<u8>(b(0x0261) - 1); n > 0; --n) addScore(0x38b1, 0x0262);
    b(0x2424) = 0x38;
    b(0x2425) = 0x37;
    if (const u8 digit = static_cast<u8>(b(0x0261) + 0x37); digit < 0x41) {
      b(0x2424) = digit;
      b(0x2425) = 0x20;
    }
    wait1();
    nextStep(4);
  });
  bindNative(0x3657, [this] {
    wait1();
    addScore(0x38b1, 0x1e30);
    nextStep(2);
  });
  bindNative(0x3681, [=, this] {  // a tall picture rolls down the display to a row of it
    nativeCW(0x369b) = arg(2);
    nativeCW(0x3699) = 0x98;
    w(0x38ff) = 0x369d;
    nextStep(4);
  });
  bindNative(0x369d, [this] {
    --nativeCW(0x3699);
    scrollPicture(nativeCW(0x3699));
    si = nativeCW(0x3699) == nativeCW(0x369b) ? 0 : 1;
  });
  simple(0x3732, [this] { ++b(0x0226); });
}

}  // namespace encore
