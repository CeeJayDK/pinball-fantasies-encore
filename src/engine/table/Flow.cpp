#include "engine/table/Flow.h"

namespace encore {

Flow::Flow(ByteView prg, int table) : Engine(prg, table) { bindFlow(); }

void Flow::chain() {
  bx = static_cast<u16>(bx + 2);
  if (nativeW(bx) == 0) {
    W(0x33e5) = 0;
    return;
  }
  W(0x33e5) = bx;
  call(nativeW(bx));
}

void Flow::goTo(u16 script) {
  bx = script;
  W(0x33e5) = bx;
  call(nativeW(bx));
}

void Flow::serve() {
  B(0x33ce) = 0;
  B(at::ballLost) = 0;
  B(0x33e0) = 0xff;
  beginBall();
  lightsOut();  // cs:009d
  stopBlinks();
  clearBall();
  restorePlayer();
  if (B(0x00ce) != 0) setLight(kb(0x0bec, 1));  // a ball more is owed: its light
  B(at::ballHidden) = 0xff;
  placeBall(kw(0x0bf7, 4), kw(0x0bfd, 4));
  W(0x3385) = 0xffff;
  if (B(0x33e3) != 0xff && B(0x00d1) != 0xff) {
    music(A(0x0c6f));
    B(0x00d1) = 0;
  }
  B(0x230a) = 0;
  B(0x33de) = 0;
  if (B(0x33e3) != 0xff) startBall();
  else addTimer(F(0x6200));
}

void Flow::startBall() {
  addTimer(F(0x0cca));
  addTimer(F(0x0cf0));
  addTimer(F(0x0ca4));
  B(0x33cf) = 0xff;
  B(at::tilted) = 0;
  W(at::tiltCounter) = 0;
  B(0x33de) = 0;
}

void Flow::bindFlow() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait1 = [this] {
    W(0x33e9) = 1;
    W(0x33e7) = F(0x5285);
  };
  auto plain = [this](int font) {
    setFont(font);
    W(0x447b) = 0;
    W(0x447d) = 0;
  };
  auto holdBonus = [this] {  // the bonus as it was before it was counted, if it is to be kept
    if (B(0x05b3) != 0xff) return;
    for (u16 i = 0; i < 12; ++i) B(0x3399, i) = B(0x33a5, i);
  };

  bind(0x0bca, [this] { serve(); });
  bind(0x6200, [this] {
    if (!countTo(A(0x3618), 0x1e)) return;
    startBall();
    endTimer();
  });
  bind(0x0ca4, [this] {
    if (!countTo(A(0x35d0), 5)) return;
    effect(0x0c1d);
    endTimer();
  });
  bind(0x0cca, [this] {
    if (!countTo(A(0x35d2), kw(0x0cca, 1))) return;
    effect(0x0c35);
    endTimer();
  });
  bind(0x0cf0, [this] {  // the ball appears at the top of the lane, rolling right
    if (!countTo(A(0x35d4), 0x50)) return;
    placeBall(kw(0x0cff, 4), kw(0x0d05, 4));
    W(at::ballVx) = kw(0x0d33, 4);
    B(at::ballHidden) = 0;
    W(0x3385) = 0xffff;
    endTimer();
  });
  // --- the bonus is counted into the score, a digit's worth at a time
  bind(0x0317, [this] {
    CW(0x0361) = 0;
    W(0x33ca) = A(0x33b3);
    W(0x33cc) = A(0x33a4);
    for (u16 i = 0; i < 12; ++i) B(0x33a5, i) = B(0x3399, i);
    W(0x33e7) = F(0x0363);
    nextStep(2);
  });
  bind(0x0363, [this] {
    si = 0x4b;
    if (++CW(0x0361) != 4) return;
    CW(0x0361) = 0;
    for (;;) {
      const u16 digit = W(0x33cc);
      if (nativeB(digit) == 0) {  // this digit is done: the next one up
        if (++W(0x33ca) == A(0x33bf)) {
          W(0x3700) = 0x498;
          W(0x3702) = 0x1c;
          W(0x3704) = 0x0a;
          fillDisplay(0x498, 0x1c, 0x0a);
          si = 0;
          return;
        }
        --W(0x33cc);
        continue;
      }
      if (--nativeB(digit) == 0) {
        bool left = false;
        for (u16 i = 0; i < 12; ++i)
          if (B(0x3399, i) != 0) left = true;
        if (left) CW(0x0361) = 0xfff6;  // a pause before the next digit
      }
      addScore(A(0x45b6), W(0x33ca));
      effect(0x0c61);
      W(0x4485) = A(0x3399);
      W(0x4487) = 0x488;
      W(0x447d) = 0;
      W(0x36fa) = F(0x456d);
      drawScore(A(0x45b6), 0xc8);
      si = 0x4b;
      return;
    }
  });
  bind(0x042c, [=, this] {
    holdBonus();
    if (++B(0x228f) >= 0x41) {
      B(0x228f) = 0x37;
      if (B(0x228e) == 0x38) ++B(0x228e);
      else B(0x228e) = 0x38;
    }
    savePlayer(currentPlayer());
    const u16 step = bx;
    addTimer(F(0x0bb7));
    bx = step;
    chain();
  });

  // --- the best scores: has any player beaten one?
  bind(0x047f, [this] {
    W(0x33e9) = 1;
    CB(0x047c) = 0;
    CW(0x047a) = 0;
    W(0x33e7) = F(0x049e);
    nextStep(4);
  });
  bind(0x049e, [=, this] {
    const u16 score = static_cast<u16>(A(0x021a) + CW(0x047a));
    CW(0x047a) += playerSize();
    if (++CB(0x047c) > B(0x3716)) {  // every player looked at
      B(0x3710) = 0;
      if (B(0x3711) != 0xff) {
        B(0x3389) = 0;
        B(0x230d) = 1;
        music(A(0x0c7e));
        B(0x3389) = 0;
        B(0x3712) = 0xff;
      }
      B(0x3711) = 0;
      si = 0;
      return;
    }
    si = 1;
    for (u16 left = 4, entry = kw(0x04af, 1); left > 0; --left, entry = static_cast<u16>(entry + 0x10)) {
      int order = 0;
      for (u16 i = 0; i < 12 && order == 0; ++i) {
        const u8 mine = nativeB(static_cast<u16>(score + i)), best = nativeB(static_cast<u16>(entry + i));
        order = mine < best ? -1 : mine > best ? 1 : 0;
      }
      if (order <= 0) continue;
      CW(0x047d) = left;
      if (B(0x3710) != 0xff) {
        B(0x3710) = 0xff;
        B(0x3389) = 0;
        B(0x230d) = 1;
        B(0x3711) = 0xff;
        music(A(0x0c81));
      }
      W(0x33e9) = 1;
      B(0x229e) = static_cast<u8>(CB(0x047c) + 0x37);
      W(0x33e7) = F(0x05ad);
      plain(0);
      drawText(A(0x2291), 0x150);
      return;
    }
  });
  bind(0x05ad, [this] {  // the scores below make room, and this one goes in
    const u16 below = static_cast<u16>((CW(0x047d) - 1) * 0x10);
    const u16 from = kw(0x05b5, 1), to = kw(0x05b8, 1);  // the last bytes of the third and fourth
    for (u16 i = 0; i < below; ++i) nativeB(static_cast<u16>(to - i)) = nativeB(static_cast<u16>(from - i));
    const u16 entry = static_cast<u16>(to - below - 0x0f);
    const u16 score = static_cast<u16>(A(0x021a) + (CB(0x047c) - 1) * playerSize());
    for (u16 i = 0; i < 12; ++i) nativeB(static_cast<u16>(entry + i)) = nativeB(static_cast<u16>(score + i));
    CW(0x0607) = 3;
    CW(0x0605) = static_cast<u16>(entry + 12);
    B(at::lastKey) = 0xff;
    W(0x33e7) = F(0x0609);
    si = 1;
  });
  bind(0x0609, [=, this] {  // three letters are typed for it
    u8 key = B(at::lastKey);
    if (key != 0xff) {
      B(at::lastKey) = 0xff;
      key = B(0x3654, key);
      if (key != 0) {
        nativeB(CW(0x0605)) = key;
        CW(0x0605) += 1;
        B(0x22a1, static_cast<u16>(3 - CW(0x0607))) = key;
        if (--CW(0x0607) == 0) {
          W(0x33e9) = 0x3c;
          W(0x33e7) = F(0x06c0);
          plain(0);
          drawText(A(0x2291), 0x150);
          for (u16 i = 0; i < 3; ++i) B(0x22a1, i) = 0x20;
          si = W(0x33e9);
          return;
        }
      }
    }
    if (redrawNameWhileWaiting()) {
      plain(0);
      drawText(A(0x2291), 0x150);
    }
    si = W(0x33e9);
  });
  bind(0x06c0, [=, this] {
    --si;
    if (si == 0x1e) {
      plain(0);
      drawText(A(0x22a6), 0x150);
    }
    if (si <= 2) {
      W(0x33e7) = F(0x049e);
      si = 1;
    }
  });

  bind(0x071c, [=, this] {  // (the next step entered past its count: sets it)
    CW(0x071a) = arg(2);
    wait1();
    nextStep(6);
  });
  bind(0x0705, [=, this] {  // back to an earlier step, so many times
    if (--CW(0x071a) != 0) return goTo(arg(4));
    CW(0x071a) = arg(2);
    wait1();
    nextStep(6);
  });

  // --- each player's score in turn
  bind(0x0735, [=, this] {
    CW(0x0733) = static_cast<u16>(-playerSize());
    B(0x2288) = 0x37;
    playerShown();
    CB(0x0732) = static_cast<u8>(B(0x3716) + 1);
    wait1();
    nextStep(4);
  });
  bind(0x0762, [=, this] {
    ++B(0x2288);
    playerShown();
    CW(0x0733) += playerSize();
    if (--CB(0x0732) == 0) {
      wait1();
      nextStep(4);
      return;
    }
    W(0x33e9) = 1;
    W(0x33e7) = F(0x07a6);
    if (CB(0x0732) == 1) return nextStep(4);
    bx = arg(2);
    W(0x33e5) = bx;
  });
  bind(0x07a6, [=, this] {
    plain(3);
    drawText(A(0x2281), 0x150);
    drawScore(static_cast<u16>(CW(0x0733) + A(0x021a)), 0xc8);
    si = 0;
  });

  // --- the match: a digit drawn by chance against the last-but-one of each score
  bind(0x07e5, [this] {
    W(0x33e7) = F(0x07fa);
    W(0x33e9) = 1;
    B(0x371a) = 1;
    nextStep(4);
  });
  bind(0x07fa, [=, this] {
    B(0x230d) = 1;
    music(A(0x0c90));
    B(0x230a) = kb(0x0806, 4);
    W(0x33e7) = F(0x089f);
    for (u16 player = 0; player < B(0x3716); ++player) {
      const u16 digit = static_cast<u16>(0x2000 | static_cast<u8>(nativeB(static_cast<u16>(A(0x021a) + 0x0a + player * playerSize())) + 0x37));
      W(0x1c16, static_cast<u16>(player * 2)) = digit;
      W(0x1c28, static_cast<u16>(player * 2)) = digit;
    }
    plain(3);
    drawText(A(0x1c16), 0xa8);
    si = matchLength();
    W(0x33e9) = si;
    matchPace();
  });
  bind(0x089f, [=, this] {
    if (--CW(0x089d) != 0) return;
    matchPace();
    plain(3);
    drawText(A(0x1c3c), static_cast<u16>(0x540 + (W(0x1c3e) << 3)));  // the last digit rubbed out
    u16 digit = W(at::loopCounter) % 10;
    if (matchAvoidsRepeat() && W(0x1c3e) == digit && ++digit >= 10) digit = 0;
    W(0x1c3e) = digit;
    B(0x1c3a) = static_cast<u8>(digit + 0x37);
    plain(3);
    W(0x36fc) = A(0x1c3a);
    W(0x36fe) = static_cast<u16>(0x540 + (digit << 3));
    W(0x36fa) = F(0x6ca5);
    if (--si != 0) return;
    // the last one stands: who has it?
    CB(0x09b6) = 0;
    for (u16 player = 0; player < B(0x3716); ++player) {
      if (B(0x1c16, static_cast<u16>(player * 2)) == static_cast<u8>(digit + 0x37)) CB(0x09b6) = 0xff;
      else W(0x1c28, static_cast<u16>(player * 2)) = 0x2a2a;
    }
    si = 0;
    if (CB(0x09b6) != 0xff) return;
    W(0x3390) = 3;
    W(0x3392) = 3;
    W(0x3394) = 0xff;
    B(0x3396) = 0xff;
    plain(3);
    drawText(A(0x1c28), 0xa8);
    B(0x3389) = 0;
    music(A(0x0c93));
    B(0x230d) = 1;
    B(0x3389) = 0;
    B(0x230a) = kb(0x0a0f, 4);
  });
  bind(0x0a19, [this] {  // the next player who matched plays a ball more
    for (;;) {
      const u16 mark = static_cast<u16>(B(0x371a) * 2 + A(0x1c14));
      const u8 digit = static_cast<u8>(nativeB(mark) - 0x37);
      nativeW(mark) = 0x5858;
      if (W(0x1c3e) == digit) {
        B(0x2288) = static_cast<u8>(B(0x371a) + 0x37);
        matchWon();
        return goTo(A(0x1790));
      }
      if (++B(0x371a) > B(0x3716)) return goTo(A(0x17b8));
    }
  });
  bind(0x0a6b, [=, this] {
    if (B(0x00cd) == 0) return chain();
    savePlayer(currentPlayer());
    if (extraBallOwed()) {
      return sameBallAgain();
    }
    if (B(0x371a) == B(0x3716)) return goTo(A(0x17b8));
    ++B(0x371a);
    B(0x2288) = static_cast<u8>(B(0x371a) + 0x37);
    goTo(A(0x17b6));
  });

  // --- after a ball: the same player again, the next player, the next ball, or the end
  bind(0x0ab2, [=, this] {
    holdBonus();
    savePlayer(currentPlayer());
    if (extraBallOwed()) {
      return sameBallAgain();
    }
    if (B(0x371a) != B(0x3716)) {
      ++B(0x371a);
    } else {
      if (++B(0x33dc) > B(0x33dd)) {  // that was the last ball
        savePlayer(currentPlayer());
        return goTo(A(0x17a8));
      }
      B(0x371a) = 1;
      B(0x228f) = static_cast<u8>(B(0x33dc) + 0x37);
      ballShown();
    }
    B(0x2288) = static_cast<u8>(B(0x371a) + 0x37);
    playerShown();
    if (restoresAtTurn()) restorePlayer();
    const u16 step = bx;
    addTimer(F(0x0bb7));
    bx = step;
    chain();
  });
  bind(0x0b43, [this] {
    const u16 step = bx;
    addTimer(F(0x0bb7));
    bx = step;
    chain();
  });
  bind(0x0b5b, [this] {  // (BX is not kept here in the original: see addTimer)
    addTimer(F(0x0b73));
    chain();
  });
  bind(0x0b73, [this] {  // the game is over: the table goes back to waiting
    B(0x3717) = 0xff;
    toAttract();
    W(0x3383) = high() ? 0x103 : 0x171;
    W(0x3718) = 0xffff;
    for (u16 i = 0; i < 12; ++i) B(0x45b6, i) = 0;
    lightsOut();
    B(0x33e3) = 0xff;
    gameOverLights();
    endTimer();
  });
  bind(0x0bb7, [this] {
    if (countTo(A(0x35ce), kw(0x0bb7, 1))) serve();
  });
}

}  // namespace encore
