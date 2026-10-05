// Party Land's rules: what happens when the ball rolls into or hits each thing on the table
// (TABLE1.PRG, cs:1104 to cs:2f9a). Routines are named by where they are in the program until
// what they are for is clearer.
#include "engine/table/PartyLand.h"

namespace encore {

void PartyLand::addBonus(u16 amount) {
  u16 times = W(0x33b1);
  do addScore(0x3399, amount);
  while (times-- > 1);
}

void PartyLand::scored() {
  B(0x33de) = 0xff;
  W(0x36f6) = 0;
  if (W(0x36f8) == 0xff) {
    startScript(0x1acc);
    W(0x36f8) = 0;
  }
}

/// The screen is taken up to the top of the table, from where it is (cs:1217, cs:1228). The
/// two timers that do it start a frame or two after the ball has been put in its hole up
/// there, and until then the screen would set off after the ball. Amended: it is held where
/// it is from this moment, and goes up from this frame's turn of the timers on.
void PartyLand::scrollUp() {
  if (!amended) return addTimer(0x1217);
  W(0x3387) = W(0x2f02);
  W(0x3383) = W(0x2f02);
  addTimer(0x1228);
}

void PartyLand::eject() {
  hole();
  scrollUp();
  addTimer(0x1175);
  W(0x3387) = W(0x2f02);
}

bool PartyLand::allFive() { return W(0x35bb) == 0xffff && W(0x35bd) == 0xffff && B(0x35bf) == 0xff; }

void PartyLand::fiveLit() {
  if (!allFive()) return;
  if (B(0x33e2) == 0xff) {
    B(0x00cb) = 0xff;
    return;
  }
  at1287();
}

void PartyLand::at1287() {
  award(0x0ba9);
  B(0x230a) = 0x2b;
  for (u8 light = 0x2a; light <= 0x2e; ++light) clearLight(light);
  if (B(0x00a2) != 0xff && B(0x00a3) != 0xff) blink(0x23, B(0x00a5), 0x0e);
  B(0x00a3) = 0xff;
  B(0x05b1) = 0xff;
  blink(0x20, 0, 8);
  B(0x33e2) = 0xff;
  B(0x33e4) = 0xff;
}

void PartyLand::at12ea(bool carry) {
  if (!carry) return;
  if (B(0x33e2) == 0xff) {
    B(0x00cc) = 0xff;
    return;
  }
  at1302();
}

void PartyLand::at1302() {
  award(0x0be1);
  B(0x230a) = 0x19;
  if (B(0x00a2) != 0xff && B(0x00a3) != 0xff) blink(0x23, B(0x00a5), 0x0e);
  B(0x00a3) = 0xff;
  B(0x05b2) = 0xff;
  blink(0x25, 0, 8);
  B(0x33e2) = 0xff;
  B(0x33e4) = 0xff;
}

void PartyLand::hitScore() {
  effect(0x0c19);
  addScore(0x45b6, 0x0184);
  at2b9d();
  addBonus(0x0190);
  B(0x33de) = 0xff;
}

/// cs:156d: true when all three drop targets are down.
bool PartyLand::at156d() {
  if (B(0x35c5) != 0 || B(0x35c6) != 0 || B(0x35c7) != 0) return false;
  award(0x0895);
  B(0x008b) = 0xff;
  B(0x008c) = 0xff;
  for (u8 light : {u8{0x10}, u8{0x12}, u8{0x18}}) stopBlink(light);
  for (u8 light : {u8{0x10}, u8{0x12}, u8{0x18}}) blink(light, 0, 2);
  addTimer(0x164b);
  if (B(0x0087, B(0x008a)) != 0xff) {
    if (B(0x008a) == 1) {
      u8 start = static_cast<u8>(B(0x0086) + 8);
      if (start >= 16) start = static_cast<u8>(start - 16);
      blink(0x11, start, 8);
      B(0x0088) = 0xff;
    } else if (B(0x008a) > 1) {
      blink(0x14, B(0x0086), 8);
      B(0x0089) = 0xff;
    } else {
      blink(0x17, B(0x0086), 8);
      B(0x0087) = 0xff;
    }
  }
  if (++B(0x008a) == 3) B(0x008a) = 0;
  return true;
}

bool PartyLand::at19c7() {
  at2420();
  B(0x00da) = award(B(0x00db) == 0xff ? 0x0a3a : 0x0a1e) ? 1 : 0;
  if (!nextOfRow(0x06ce)) return false;
  for (u8 light : {u8{0x1c}, u8{0x1f}, u8{0x22}, u8{0x26}, u8{0x29}}) clearLight(light);
  return true;
}

void PartyLand::at1b19() {
  stopBlink(0x0b);
  lightOff(0x0b);
  B(0x009a) = 0;
}

void PartyLand::at1c82() {
  if (B(0x35bb) == 0xff) return;
  award(0x070d);
  setLight(0x2a);
  fiveLit();
}

/// What the arcade gives. The original goes by a count of the frames played, through a list
/// of 128 (ds:05b4), and gives it a frame later. Amended: one of the six by chance proper,
/// each as likely as the others, and at once.
void PartyLand::at1ddb() {
  if (!amended) return addTimer(W(0x0634, B(0x05b4, static_cast<u16>(B(0x0093) >> 1))));
  static constexpr u16 kPrizes[6] = {0x1dfa, 0x1e27, 0x1e62, 0x1e95, 0x1ec8, 0x1f0a};
  const u16 running = W(0x3381);
  addTimer(kPrizes[chance(6, 0)]);
  W(0x3381) = bx;
  call(nativeW(bx));  // (each of them ends itself)
  W(0x3381) = running;
}

void PartyLand::at1efb() {
  W(0x05af) = 0x0a;
  addTimer(0x1104);
  endTimer();
}

bool PartyLand::at232a() {
  if (B(0x00a0) != 0xff) return false;
  award(0x0879);
  stopBlink(0x1b);
  lightOff(0x1b);
  B(0x00a0) = 0;
  W(0x00a6) = B(0x33e2) == 0xff ? 0x0f : 0xa0;
  return true;
}

bool PartyLand::at2364() {
  if (B(0x00a1) != 0xff) return false;
  stopBlink(0x1e);
  lightOff(0x1e);
  setLight(0x33);
  ++B(0x00ce);
  award(0x09e6);
  B(0x00a1) = 0;
  W(0x00a6) = B(0x33e2) == 0xff ? 0x0f : 0x140;
  return true;
}

bool PartyLand::at23a7() {
  if (B(0x00a2) != 0xff && B(0x00a3) != 0xff) return false;
  B(0x00a2) = 0;
  B(0x00a3) = 0;
  stopBlink(0x23);
  lightOff(0x23);
  music(0x0ca2);
  startScript(B(0x33e2) != 0xff ? 0x1647 : B(0x05b1) != 0xff ? 0x166f : 0x1651);
  addScore(0x45b6, 0x010c);
  for (u16 i = 0; i < 12; ++i) B(0x010c, i) = B(0x0124, i);
  W(0x00a6) = 0x19a;
  return true;
}

void PartyLand::at26b0() {
  if (B(0x35be) == 0xff) return;
  setLight(0x2d);
  award(0x0761);
  fiveLit();
}

bool PartyLand::laneScore() {
  addScore(0x45b6, 0x019c);
  scored();
  addBonus(0x01a8);
  at2420();
  return B(0x3592) == 0xff && B(0x3593) == 0xff && B(0x3595) == 0xff && B(0x3596) == 0xff;
}

/// cs:28f5: all four lanes lit.
void PartyLand::at28f5() {
  auto later = [](u8 start, u8 half) { start = static_cast<u8>(start + half); return start >= half * 2 ? static_cast<u8>(start - half * 2) : start; };
  blink(1, B(0x00a4), 2);
  blink(5, B(0x00a4), 2);
  blink(2, later(B(0x00a4), 2), 2);
  blink(4, later(B(0x00a4), 2), 2);
  addTimer(0x28bb);
  ++W(0x009e);
  if (W(0x009e) == 1) {
    if (B(0x00a0) != 0xff) {
      blink(0x1b, B(0x00a5), 0x0e);
      B(0x00a0) = 0xff;
    }
  } else if (W(0x009e) == 2) {
    if (B(0x00a1) != 0xff) {
      blink(0x1e, later(B(0x00a5), 0x0e), 0x0e);
      B(0x00a1) = 0xff;
    }
  } else if (W(0x009e) == 3 && B(0x00a2) != 0xff) {
    if (B(0x00a3) != 0xff) blink(0x23, B(0x00a5), 0x0e);
    B(0x00a2) = 0xff;
  }
  if (B(0x35bf) != 0xff) {
    setLight(0x2e);
    award(0x077d);
    fiveLit();
  }
}

void PartyLand::at2a94() {
  if (B(0x35bd) != 0xff) {
    setLight(0x2c);
    award(0x0745);
    fiveLit();
  }
  at1b19();
}

void PartyLand::at2b9d() {
  if (B(0x05b1) != 0xff) return;
  addScore(0x00f4, 0x016c);
  B(0x33df) = 0xff;
}

void PartyLand::at2bb7() {
  if (B(0x05b2) != 0xff) return;
  addScore(0x0100, 0x0178);
  B(0x33df) = 0xff;
}

void PartyLand::at2540() {
  clearLight(0x27);
  award(0x0a02);
  setLight(0x33);
  ++B(0x00ce);
  B(0x00d2) = 0xff;
  addTimer(0x2560);
}

void PartyLand::countdownTick() {
  const bool nine = B(0x36f3) == 0x2a && B(0x36f4) == 0x39, nought = B(0x36f3) == 0x2a && B(0x36f4) == 0x37;
  if (nine && B(0x00a2) != 0xff && B(0x00a3) == 0xff) {
    stopBlink(0x23);
    blink(0x23, 0, 2);
  }
  if (!nought) return;
  const bool first = B(0x05b1) == 0xff;
  if (first) {
    W(0x33e9) = 0;
    B(0x33e2) = 0;
    B(0x05b1) = 0;
  } else {
    B(0x33e2) = 0;
    B(0x05b2) = 0;
    W(0x33e9) = 0;
  }
  const u8 light = first ? 0x20 : 0x25;
  stopBlink(light);
  lightOff(light);
  award(first ? 0x0bc5 : 0x0bfd);
  B(0x230a) = 1;
  stopBlink(0x23);
  lightOff(0x23);
  B(0x00a3) = 0;
  if (B(0x00a2) == 0xff) blink(0x23, B(0x00a5), 0x0e);
  if (first) {
    if (B(0x00cc) == 0xff) {
      B(0x00cc) = 0;
      addTimer(0x2f55);
    }
  } else if (B(0x00cb) == 0xff) {
    B(0x00cb) = 0;
    addTimer(0x2f40);
  }
}

void PartyLand::bindRules() {
  // a timer that waits `limit` frames and then does something once
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bind(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };
  bind(0x2ddc, [this] { countdownTick(); });

  bind(0x1104, [this] {
    if (B(0x33e2) != 0xff && !countTo(0x35d6, W(0x05af))) return;
    eject();
    endTimer();
  });
  after(0x1175, 0x35d8, 0x1e, [this] {
    blink(0x03, 0, 7);
    blink(0x38, 0, 7);
    addTimer(0x119f);
    endTimer();
  });
  after(0x119f, 0x35da, 0x1b, [this] {  // out of the hole, upwards on the ramps' side
    W(0x3383) = 0xffff;
    const u16 speed = chance(0x80, W(at::loopCounter) & 0x7f);
    hole();
    B(at::layer) = 0xff;
    W(at::ballVy) = speed;
    effect(0x0c35);
    stopBlink(0x03);
    stopBlink(0x38);
    endTimer();
  });
  bind(0x1217, [this] {
    addTimer(0x1228);
    W(0x3387) = W(0x2f02);
    endTimer();
  });
  bind(0x1228, [this] {  // the screen goes up to the top of the table
    W(0x3387) -= 5;
    if (W(0x3387).s() < 0) {
      W(0x3383) = 0;
      endTimer();
    } else {
      W(0x3383) = W(0x3387);
    }
  });
  bind(0x134c, [this] {
    if (B(0x0094) == 0xff) return;
    effect(0x0c5d);
    B(0x0094) = 0xff;
    addTimer(0x1397);
    if (B(0x008e) == 0xff) return;
    B(0x008e) = 0xff;
    blink(0x07, 0, 0x0c);
    blink(0x37, 0, 0x0c);
  });
  after(0x1397, 0x35dc, 0x14, [this] {
    B(0x0094) = 0;
    endTimer();
  });

  // the three drop targets
  struct Target { u16 hit, lit, down, light, fall, counter, shape, at, width, flash, flashCount, flashTimer; };
  for (const Target t : {Target{0x13ae, 0x35c5, 0x008b, 0x34, 0x1419, 0x35de, 0x68d0, 0x2b5a, 2, 0x10, 0x13f5, 0x13f6},
                         Target{0x142e, 0x35c6, 0x008c, 0x35, 0x1499, 0x35e0, 0x6910, 0x2e2b, 2, 0x12, 0x1475, 0x1476},
                         Target{0x14ae, 0x35c7, 0x008d, 0x36, 0x1519, 0x35e2, 0x6930, 0x30fc, 1, 0x18, 0x14f5, 0x14f6}}) {
    bind(t.hit, [this, t] {
      if (B(t.lit) != 0xff || B(t.down) == 0xff) return;
      B(t.down) = 0xff;
      clearLight(static_cast<u8>(t.light));
      addTimer(t.fall);
      hitScore();
      if (at156d()) return;
      addTimer(t.flashTimer);
      blink(static_cast<u8>(t.flash), 0, 3);
      CB(t.flashCount) = 0;
    });
    bind(t.flashTimer, [this, t] {
      if (CB(t.flashCount) == 0x0d) {
        stopBlink(static_cast<u8>(t.flash));
        B(t.down) = 0;
        dropTimer();
      }
      ++CB(t.flashCount);
    });
    after(t.fall, t.counter, 0x14, [this, t] {  // it goes down: no longer in the ball's way
      patchMask(0x3b74, t.at, t.shape, t.width, 0x0f);
      endTimer();
    });
  }
  after(0x164b, 0x35e4, 0x47, [this] {  // and all three stand again
    for (u8 light : {u8{0x10}, u8{0x12}, u8{0x18}}) stopBlink(light);
    setLight(0x34);
    patchMask(0x3b74, 0x2b5a, 0x68b0, 2, 0x0f);
    lightOff(0x10);
    setLight(0x35);
    patchMask(0x3b74, 0x2e2b, 0x68f0, 2, 0x0f);
    lightOff(0x12);
    setLight(0x36);
    patchMask(0x3b74, 0x30fc, 0x6940, 1, 0x0f);
    lightOff(0x18);
    effect(0x0c1d);
    B(0x008b) = 0;
    B(0x008c) = 0;
    B(0x008d) = 0;
    endTimer();
  });

  bind(0x16b6, [this] {
    if (W(0x3318) != 0x18af) return;
    if (W(0x05a9) != 0) {
      W(0x05a9) = 0;
      return;
    }
    if (B(0x00c9) == 0xff) return;
    at2420();
    at2bb7();
    if (W(0x05ad) != 0) at26b0();
    W(0x05ad) = 0x258;
    W(0x05ab) = 0x12c;
    ++B(0x00c8);
    if (B(0x00c8) < 2) {
      stopBlink(0x1a);
      lightOn(0x1a);
      blink(0x19, 0, 9);
      award(0x0825);
    } else if (B(0x00c8) == 2) {
      stopBlink(0x19);
      lightOn(0x19);
      blink(0x13, 0, 9);
      award(0x0841);
    } else {
      B(0x00c8) = 0;
      stopBlink(0x13);
      blink(0x13, 0, 2);
      blink(0x19, 0, 2);
      blink(0x1a, 0, 2);
      award(0x085d);
      addTimer(0x1880);
      B(0x00c9) = 0xff;
    }
    if (B(0x0097) == 0xff) {
      B(0x0097) = 0;
      stopBlink(0x1d);
      lightOff(0x1d);
      struct Step { u16 lit; u8 light; u16 prize, times; };
      for (const Step s : {Step{0x35c0, 0x2f, 0x093e, 2}, {0x35c2, 0x31, 0x095a, 4}, {0x35c3, 0x32, 0x0976, 6}, {0x35c1, 0x30, 0x0992, 8}}) {
        if (B(s.lit) == 0xff) continue;
        setLight(s.light);
        award(s.prize);
        W(0x33b1) = s.times;
        B(0x01fc) += 2;
        if (B(0x01fc) == 3) --B(0x01fc);
        if (++B(0x2238) != 0x39) ++B(0x2238);
        break;
      }
    }
    if (B(0x0098) == 0xff) {
      B(0x0098) = 0;
      stopBlink(0x21);
      lightOff(0x21);
      award(0x09ae);
      B(0x05b3) = 0xff;
    }
    if (B(0x0099) == 0xff) {
      B(0x0099) = 0;
      stopBlink(0x24);
      lightOff(0x24);
      award(0x09ca);
      addScore(0x3399, 0x3399);
    }
  });
  after(0x1880, 0x35e6, 0x78, [this] {
    for (u8 light : {u8{0x13}, u8{0x19}, u8{0x1a}}) stopBlink(light);
    blink(0x1a, 0, 9);
    B(0x00c9) = 0;
    endTimer();
  });
  bind(0x18af, [this] {
    if (W(0x3318) != 0x16b6 || B(0x00ca) == 0xff) return;
    at2420();
    at2bb7();
    if (W(0x05ad) != 0) at26b0();
    W(0x05ad) = 0x258;
    if (B(0x3597) != 0xff) {
      award(0x0a56);
      blink(0x06, 0, 2);
      addTimer(0x1900);
    } else if (B(0x3599) != 0xff) {
      award(0x0a72);
      blink(0x08, 0, 2);
      addTimer(0x1940);
    } else {
      B(0x3597) = 0;
      B(0x3599) = 0;
      blink(0x06, 0, 2);
      blink(0x08, 0, 2);
      blink(0x09, 0, 2);
      addTimer(0x19a1);
      B(0x00ca) = 0xff;
      award(0x0a8e);
      B(0x00db) = 0;
      at12ea(at19c7());
    }
  });
  after(0x1900, 0x35e8, 0x0e, [this] {
    stopBlink(0x06);
    B(0x3597) = 0xff;
    endTimer();
  });
  after(0x1940, 0x35ea, 0x0e, [this] {
    stopBlink(0x08);
    B(0x3599) = 0xff;
    endTimer();
  });
  after(0x19a1, 0x35ec, 0x78, [this] {
    for (u8 light : {u8{0x06}, u8{0x08}, u8{0x09}}) stopBlink(light);
    B(0x00ca) = 0;
    endTimer();
  });

  bind(0x1a0e, [this] {
    B(0x33f8) = 0;
    award(0x0b37);
    at2420();
    blink(0x0b, 0, 6);
    B(0x009a) = 0xff;
    addTimer(0x1aaf);
    hole();
  });
  bind(0x1a6c, [this] {
    hole();
    eject();
    stopBlink(0x0b);
  });
  bind(0x1aaf, [this] {
    if (B(0x33f8) != 0xff) return;
    addTimer(0x1ac7);
    eject();
    endTimer();
  });
  after(0x1ac7, 0x35ee, 0x1e0, [this] {
    stopBlink(0x0b);
    if (B(0x009a) != 0) {
      blink(0x0b, 0, 2);
      addTimer(0x1b04);
    } else {
      lightOff(0x0b);
    }
    endTimer();
  });
  after(0x1b04, 0x35f0, 0x78, [this] {
    at1b19();
    endTimer();
  });
  bind(0x1b29, [this] {
    at2420();
    at2bb7();
    if (W(0x00aa) != 0) {
      at2420();
      W(0x00aa) = 0;
      addScore(0x00dc, 0x016c);
      addScore(0x45b6, 0x00dc);
      award(0x0b70);
      B(0x3398) = 0xff;
      at1c82();
    } else if (W(0x05ab) != 0) {
      at1c82();
    }
    if (B(0x359d) == 0xff) {
      award(0x07d1);
      W(0x00a8) = 0x5a0;
    } else if (B(0x359f) == 0xff) {
      award(0x07b5);
      W(0x00a8) = 0x5a0;
      stopBlink(0x0c);
      setLight(0x0c);
      blink(0x0a, 0, 8);
    } else {
      award(0x0799);
      W(0x00a8) = 0x2d0;
      stopBlink(0x0e);
      setLight(0x0e);
      blink(0x0c, 0, 8);
    }
    W(0x00ac) = 0x82;
    scrollUp();
    addTimer(0x1c0f);
    if (B(0x33e2) != 0xff) addTimer(0x1c36);
    B(0x3398) = 0;
  });
  bind(0x1c0f, [this] {
    if (B(0x33e2) != 0xff && !countTo(0x35f2, W(0x00ac))) return;
    W(0x3383) = 0xffff;
    eject();
    endTimer();
  });
  after(0x1c36, 0x35f4, 2, [this] {
    hole();
    endTimer();
  });
  bind(0x1c9c, [this] {
    hole();
    eject();
  });
  bind(0x1cda, [this] {
    B(0x0093) += 0x15;
    at2bb7();
    if (B(at::tilted) == 0xff || B(0x008e) != 0xff) {
      eject();
      return;
    }
    B(0x008e) = 0;
    stopBlink(0x07);
    stopBlink(0x37);
    lightOff(0x07);
    lightOff(0x37);
    B(0x230b) = B(0x230a);
    B(0x00d3) = 0;
    W(0x3551) = 0x19;
    if (!award(0x06f1)) {
      B(0x00d3) = 0xff;
      at1ddb();
      B(0x230b) = B(0x230a);
    } else {
      B(0x230a) = 0x3e;
      // (amended: the arcade's jingle keeps its own priority while it plays)
      if (!amended) B(0x3389) = 1;
      B(0x338f) = 0;
      B(0x338e) = 0;
      addTimer(0x1da2);
    }
    scrollUp();
    hole();
  });
  bind(0x1da2, [this] {
    if (B(0x33e2) != 0xff && B(0x00d3) != 0xff) return;
    if (B(at::tilted) == 0xff) {
      eject();
      endTimer();
      return;
    }
    // (amended: after the arcade the music starts again from the beginning of the table's tune,
    // not from where it was when the ball went in)
    if (amended) B(0x230b) = kb(0x34f7, 4);
    B(0x230a) = B(0x230b);
    // (amended: the prize's jingle is not cut short by one of less weight: the priority
    // stays, and a jingle still to be repeated is)
    if (!amended) {
      B(0x230d) = 1;
      B(0x3389) = 0;
    } else if (B(0x230d) == 0) {
      B(0x230d) = 1;
    }
    at1ddb();
    endTimer();
  });
  // what the hole gives, one of these by chance (the table at ds:0634)
  auto prize = [this](u16 record, u16 show, u16 hold) {
    if (!award(record)) return at1efb();
    B(0x230a) = B(0x230b);
    if (!amended) B(0x3389) = 0;  // (amended: a prize's jingle keeps its priority)
    W(0x00d5) = show;
    W(0x00d8) = hold;
    B(0x00d7) = 0;
    addTimer(0x1f32);
    endTimer();
  };
  bind(0x1dfa, [this] {
    setLight(0x27);
    if (!award(0x0640)) return at1efb();
    B(0x230a) = B(0x230b);
    if (!amended) B(0x3389) = 0;  // (amended: a prize's jingle keeps its priority)
    W(0x05af) = 0xa0;
    addTimer(0x1104);
    endTimer();
  });
  bind(0x1e27, [this] {
    B(0x00db) = 0xff;
    at12ea(at19c7());
    if (B(0x00da) == 0) return at1efb();
    B(0x230a) = B(0x230b);
    if (!amended) B(0x3389) = 0;  // (amended: a prize's jingle keeps its priority)
    W(0x00d5) = 0x8c;
    W(0x00d8) = 0xb4;
    B(0x00d7) = 0;
    addTimer(0x1f32);
    endTimer();
  });
  bind(0x1e62, [=] { prize(0x0679, 0x78, 0x96); });
  bind(0x1e95, [=] { prize(0x065d, 0x6e, 0x8c); });
  bind(0x1ec8, [=] { prize(0x0695, 0x2d, 0x46); });
  bind(0x1f0a, [this] {
    if (award(0x06b1)) {
      B(0x230a) = B(0x230b);
      if (!amended) B(0x3389) = 0;
    }
    W(0x05af) = 0x2d;
    addTimer(0x1104);
    endTimer();
  });
  bind(0x1f32, [this] {
    bool done = --W(0x00d8) == 0;
    if (amended) {
      // The original lets the ball go when the prize's jingle is over and its time on the
      // display is up. Amended: when its time is up, once the jingle has been heard to play;
      // a long jingle no longer keeps the ball back.
      if (!done && B(0x33e2) != 0xff) {
        if (W(0x00d5) != 0) --W(0x00d5);
        if (B(0x230d) == 0 && B(0x00d7) != 0xff) return;
        B(0x00d7) = 0xff;
        if (W(0x00d5) != 0) return;
      }
    } else if (!done) {
      if (W(0x00d5) != 0) --W(0x00d5);
      if (B(0x00d7) != 0xff) {
        if (B(0x33e2) == 0xff) done = true;
        else if (B(0x338f) != 0xff) return;
      }
      if (!done) {
        B(0x00d7) = 0xff;
        if (W(0x00d5) != 0) return;
      }
    }
    B(0x338f) = 0;
    W(0x00d5) = 0xffff;
    W(0x00d8) = 0x0a;
    eject();
    endTimer();
  });

  bind(0x1f92, [this] {
    if (B(0x0095) == 0xff) return;
    B(0x0095) = 0xff;
    addScore(0x45b6, 0x01c0);
    scored();
    addBonus(0x01cc);
    at2bb7();
    if (B(0x0089) == 0xff) {
      award(0x08e9);
      at2420();
      ++B(0x0096);
    } else if (B(0x0088) == 0xff) {
      award(0x08cd);
      at2420();
    } else if (B(0x0087) == 0xff) {
      award(0x08b1);
      at2420();
    } else {
      award(0x0809);
    }
    for (u8 light : {u8{0x17}, u8{0x11}, u8{0x14}}) {
      stopBlink(light);
      lightOff(light);
    }
    if (B(0x0089) == 0xff) {
      if (B(0x35bc) != 0xff) {
        setLight(0x2b);
        award(0x0729);
        fiveLit();
      }
      if (B(0x0096) <= 1) {
        if (B(0x0098) != 0xff) {
          B(0x0098) = 0xff;
          u8 start = static_cast<u8>(B(0x009b) + 0x0c);
          if (start >= 0x18) start = static_cast<u8>(start - 0x18);
          blink(0x21, start, 0x0c);
        }
      } else if (B(0x0099) != 0xff) {
        B(0x0099) = 0xff;
        addTimer(0x20e1);
        blink(0x24, B(0x009b), 0x0c);
      }
    }
    B(0x0087) = 0;
    B(0x0088) = 0;
    B(0x0089) = 0;
    addTimer(0x2137);
  });
  after(0x20e1, 0x35f6, 0x1e0, [this] {
    if (B(0x0099) == 0xff) {
      stopBlink(0x24);
      blink(0x24, 0, 2);
      addTimer(0x2111);
    }
    endTimer();
  });
  after(0x2111, 0x35f8, 0x78, [this] {
    if (B(0x0099) == 0xff) {
      stopBlink(0x24);
      B(0x0099) = 0;
    }
    endTimer();
  });
  auto held = [this] {  // the ball kept at the top of the left ramp
    B(at::ballHidden) = 0xff;
    placeBall(0x03, 0xfd);
    B(at::layer) = 0xff;
  };
  bind(0x2137, [=, this] {
    held();
    if (!countTo(0x35fa, 0x28)) return;
    addTimerNow(0x218f);
    endTimer();
  });
  bind(0x218f, [=, this] {
    held();
    if (B(0x33e2) != 0xff && !countTo(0x35fc, 0x5a)) return;
    effect(0x0c41);
    held();
    B(at::ballHidden) = 0;
    W(at::ballVy) = 0xf63c;
    addTimer(0x2246);
    endTimer();
  });
  after(0x2246, 0x35fe, 0x3c, [this] {
    B(0x0095) = 0;
    endTimer();
  });
  bind(0x225d, [this] { W(0x05a9) = 0; });
  bind(0x2264, [this] {  // the ball came back down the lane: it is given again
    if (W(0x3318) != 0x25c2) return;
    B(0x33e3) = 0;
    music(0x0c72);
    B(0x230a) = 1;
    startScript(0x148d);
    B(0x33e0) = 0;
    B(0x00d0) = 0;
    B(0x00d1) = 0;
  });
  bind(0x2299, [this] {
    if (B(0x00d4) == 0xff) return;
    B(0x00d4) = 0xff;
    placeBall(0x101, 0x136);
    B(at::ballHidden) = 0xff;
    CB(0x2298) = 0;
    at2bb7();
    if (at232a()) ++CB(0x2298);
    if (at2364()) ++CB(0x2298);
    if (at23a7()) ++CB(0x2298);
    if (CB(0x2298) == 0) {
      award(0x07ed);
      W(0x00a6) = 0x55;
    }
    addTimer(0x242a);
  });
  bind(0x242a, [this] {
    if (!countTo(0x3600, W(0x00a6))) return;
    blink(0x15, 0, 7);
    addTimer(0x244c);
    endTimer();
  });
  after(0x244c, 0x3602, 0x1b, [this] {
    stopBlink(0x15);
    effect(0x0c35);
    B(at::ballHidden) = 0;
    placeBall(0x101, 0x136);
    W(at::ballVy) = 0x627;
    W(at::ballVx) = 0xfdc1;
    B(0x00d4) = 0;
    endTimer();
  });
  for (u16 at : {u16{0x24bc}, u16{0x24d7}})
    bind(at, [this] {
      award(0x0b8c);
      effect(0x0c49);
    });
  for (u16 at : {u16{0x24f2}, u16{0x2577}})
    bind(at, [this] {
      if (B(0x35b8) == 0xff) return at2540();
      effect(0x0c45);
      addScore(0x45b6, 0x01b4);
      scored();
    });
  after(0x2560, 0x3604, 0x258, [this] {
    B(0x00d2) = 0;
    endTimer();
  });
  bind(0x25c2, [this] {
    W(0x00aa) = 0x12c;
    B(0x338a) = 0;
    W(0x05a9) = 0x78;
  });
  bind(0x25d5, [this] { B(0x338a) = 0xff; });
  bind(0x25dc, [this] {
    if (W(0x3318) != 0x26ca) return;
    at2bb7();
    patchMask(0x46b4, 0x0266, 0x1356, 2, 0x12);
    if (W(0x05ad) != 0) at26b0();
    W(0x05ad) = 0x258;
    ++W(0x009c);
    if (W(0x009c) >= 3) {
      award(0x0ae3);
      blink(0x16, 0, 2);
      blink(0x0d, 0, 2);
      blink(0x0f, 0, 2);
      addTimer(0x268f);
      W(0x009c) = 0;
      if (B(0x0097) != 0xff && B(0x35c1) != 0xff) {
        B(0x0097) = 0xff;
        blink(0x1d, B(0x009b), 0x0c);
        award(0x0921);
      }
    } else if (W(0x009c) == 1) {
      lightOn(0x16);
      award(0x0aab);
    } else {
      award(0x0ac7);
      lightOn(0x0d);
    }
  });
  after(0x268f, 0x3606, 0x78, [this] {
    for (u8 light : {u8{0x16}, u8{0x0d}, u8{0x0f}}) stopBlink(light);
    endTimer();
  });
  bind(0x26ca, [this] { patchMask(0x46b4, 0x0266, 0x1332, 2, 0x12); });

  // the four lanes
  struct Lane { u16 roll, lit; u8 light; u16 timer, counter; };
  for (const Lane l : {Lane{0x26ce, 0x3592, 1, 0x270f, 0x3608}, {0x272b, 0x3593, 2, 0x276c, 0x360a}, {0x2788, 0x3595, 4, 0x27c9, 0x360c},
                       {0x27e5, 0x3596, 5, 0x2826, 0x360e}}) {
    bind(l.roll, [this, l] {
      effect(0x0c45);
      if (B(l.lit) != 0) return;
      B(l.lit) = 0xff;
      if (laneScore()) return at28f5();
      B(0x00cf) = 0xff;
      blink(l.light, 0, 2);
      addTimer(l.timer);
    });
    after(l.timer, l.counter, 0x0d, [this, l] {
      stopBlink(l.light);
      B(0x00cf) = 0;
      endTimer();
    });
  }
  after(0x28bb, 0x3610, 0x64, [this] {
    for (u8 light : {u8{1}, u8{2}, u8{4}, u8{5}}) stopBlink(light);
    for (u8 light : {u8{1}, u8{2}, u8{4}, u8{5}}) clearLight(light);
    endTimer();
  });
  bind(0x29e7, [] {});
  bind(0x29e8, [] {});
  bind(0x29e9, [this] {
    at2bb7();
    if (W(0x00aa) != 0) {
      at2420();
      at2420();
      if (!amended) award(0x0b54);  // (the original gives the skill shot's award twice; amended: once)
      addScore(0x00e8, 0x016c);
      addScore(0x45b6, 0x00e8);
      award(0x0b54);
      B(0x3398) = 0xff;
      at2a94();
    } else if (W(0x05ab) != 0) {
      at2a94();
    }
    if (B(0x009a) == 0xff) {
      W(0x00ae) += 5;
      addScore(0x00b0, 0x013c);
      addScore(0x00bc, 0x0154);
      award(0x0b1b);
      at1b19();
      at1b19();
    } else {
      do {
        ++W(0x00ae);
        addScore(0x00b0, 0x0130);
        addScore(0x00bc, 0x0160);
      } while (W(0x00ae) == 1);
      award(0x0aff);
      at1b19();
    }
    B(0x3398) = 0;
  });
  after(0x2f40, 0x3612, 0x190, [this] {
    at1287();
    endTimer();
  });
  after(0x2f55, 0x3614, 0x190, [this] {
    at1302();
    endTimer();
  });
}

/// The steps Party Land adds to the display's scripts (cs:2bf4 on).
void PartyLand::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait1 = [this] {
    W(0x33e9) = 1;
    W(0x33e7) = 0x5285;
  };
  bind(0x2bf4, [=, this] {
    B(0x00d0) = 0xff;
    B(0x1cf9) = static_cast<u8>(B(0x371a) + 0x37);
    wait1();
    nextStep(4);
  });
  bind(0x2c14, [=, this] {
    B(0x1d91) = static_cast<u8>(B(0x371a) + 0x37);
    wait1();
    nextStep(4);
  });
  bind(0x52cc, [] {});
  bind(0x2c2f, [=, this] {
    B(0x00d0) = 0xff;
    W(0x33e9) = 1;
    W(0x33e7) = 0x52cc;
    nextStep(4);
  });
  bind(0x2c43, [=, this] {
    wait1();
    nextStep(4);
  });
  bind(0x2c52, [=, this] {
    B(0x00d3) = 0xff;
    wait1();
    nextStep(4);
  });
  bind(0x2c66, [=, this] {
    W(0x33e7) = 0x2c75;
    W(0x33e9) = 0x2c;
    nextStep(4);
  });
  bind(0x2c75, [this] { si = B(0x33e3) == 0xff ? 0x22 : 0; });
  bind(0x2c88, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < 12; ++i)
      if (nativeB(static_cast<u16>(arg(2) + i)) != 0) nought = false;
    bx = nought ? arg(4) : static_cast<u16>(bx + 6);
    if (nativeW(bx) == 0) {
      W(0x33e5) = 0;
      return;
    }
    W(0x33e5) = bx;
    call(nativeW(bx));
  });
  bind(0x2ccd, [=, this] {
    if (B(0x01fc) > 1) {
      wait1();
      nextStep(4);
      return;
    }
    bx = arg(2);
    W(0x33e5) = bx;
    call(nativeW(bx));
  });
  bind(0x2cfd, [=, this] {  // the bonus times its multiplier
    for (u16 i = 0; i < 12; ++i) B(0x01fd, i) = B(0x3399, i);
    for (u8 n = static_cast<u8>(B(0x01fc) - 1); n > 0; --n) addScore(0x3399, 0x01fd);
    B(0x1db9) = static_cast<u8>(B(0x01fc) + 0x37);
    wait1();
    nextStep(4);
  });
  bind(0x2d3f, [=, this] {
    wait1();
    addScore(0x3399, 0x0100);
    nextStep(2);
  });
  bind(0x2d69, [=, this] {
    wait1();
    addScore(0x3399, 0x00f4);
    nextStep(2);
  });
  bind(0x2d93, [=, this] {
    for (u16 i = 0; i < 12; ++i) B(0x00bc, i) = 0;
    wait1();
    u32 times = W(0x00ae);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(0x00bc, 0x0160);
    addScore(0x3399, 0x00bc);
    nextStep(2);
  });
  bind(0x2f6a, [=, this] {
    B(0x33f8) = 0xff;
    wait1();
    nextStep(4);
  });
  bind(0x2f7f, [=, this] {
    wait1();
    ++B(0x00ce);
    setLight(0x33);
    nextStep(4);
  });
}

}  // namespace encore
