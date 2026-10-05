// The frame: the driver's two callbacks, the program's own loop, the keyboard, the timers.
#include "engine/table/Engine.h"

namespace encore {

Engine::Engine(ByteView prg, int table) : Program(prg, table) {
  for (auto& plane : video_) plane.assign(0x10000, 0);
  bind(0x69fc, [] {});  // the original's "nothing": a lone ret, which empty slots point at
  bindDisplay();
  bindGame();
  bindPlunger();
}

void Engine::effect(u16 record) {
  sound->effect(B(record), B(record, 1), 0, static_cast<u8>(B(record, 3) + 1));
}

void Engine::frame() {
  frameCallback();
  midFrameCallback();
  for (int i = 0; i < loopsPerFrame; ++i) mainLoop();
}

// ---------------------------------------------------------------------------------------
// cs:3e44, the keyboard interrupt
// ---------------------------------------------------------------------------------------
void Engine::key(u8 al) {
  if (!(al & 0x80)) {
    B(at::lastKey) = al;
    CB(0x3476) = 0x0f;
  }
  if (CB(0x3f89) == 0xff) return keyExtended(al);
  if (al == 0xe0) {
    CB(0x3f89) = 0xff;
    return;
  }
  if (al == 0x57) { B(0x3553) = 0x0b; return; }  // F11
  if (al == 0x58) { B(0x3553) = 0x0c; return; }  // F12
  // left shift, alt, ctrl: the left flipper
  if ((al == 0x2a || al == 0x38 || al == 0x1d) && !(B(at::keys) & 2)) {
    B(at::keys) |= 2;
    B(0x338b) = 0xff;
    if (B(0x33cf) != 0) effect(0xc2d);
    return;
  }
  if (al == 0x36 && !(B(at::keys) & 1)) {  // right shift: the right flipper
    B(at::keys) |= 1;
    B(0x338b) = 0xff;
    if (B(0x33cf) != 0) effect(0xc2d);
    return;
  }
  if (al == 0xb6) { B(at::keys) &= 0xfe; return; }
  if (al == 0xaa || al == 0xb8 || al == 0x9d) { B(at::keys) &= 0xfd; return; }
  if (al == 0x39) { B(0x2310) = 0xff; return; }  // space: the nudge, held
  if (al == 0xb9) {
    B(0x2310) = 0;
    CB(0x3477) = 0;
  }
}

/// cs:3f8a: the key after an 0xe0, which is how the arrows and the right-hand alt and ctrl
/// arrive.
void Engine::keyExtended(u8 al) {
  CB(0x3f89) = 0;
  if (B(0x3713) == 0xff) {  // no game: page up and page down choose the mid-frame steps
    if (al == 0x49) B(at::keys) &= 0xfb;
    else if (al == 0x51) B(at::keys) |= 4;
    return;
  }
  switch (al) {
    case 0x50:  // down arrow: the plunger is pulled
      W(at::plungerRoutine) = F(0x5e0d);
      B(0x354f) = 0xff;
      break;
    case 0xd0:  // and let go
      W(at::plungerRoutine) = F(0x5e6a);
      B(0x354f) = 0;
      break;
    case 0x38: case 0x1d:  // right alt, right ctrl
      if (!(B(at::keys) & 1)) {
        B(at::keys) |= 1;
        B(0x338b) = 0xff;
        if (B(0x33cf) != 0) effect(0xc2d);
      }
      break;
    case 0xb8: case 0x9d: B(at::keys) &= 0xfe; break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------
// cs:41c6, the callback at the start of the frame
// ---------------------------------------------------------------------------------------
void Engine::frameCallback() {
  ++W(0x354b);
  B(0x354d) = 0xff;
  B(0x354e) = 0xff;
  if (B(0x2f29) != 0xff) return;
  B(0x36f2) = 0xff;  // the driver says no frame was missed
  if (B(0x33f0) != 0) --B(0x33f0);
  W(at::loopCounter) += 0x406;
  if (B(0x3713) == 0xff) return attractFrame();
  B(0x230e) = 0;
  if (CB(0x444b) == 0xff) return;
  const u8 busy = B(0x23aa);
  B(0x23aa) = 0xff;
  if (busy == 0xff) return;

  // Which of the ball and the flippers is drawn before the sub-steps and which after depends
  // on how far down the screen each is; the flags are kept as the original keeps them.
  B(0x2f0a) = 0;
  B(0x2f0b) = 0;
  const u16 ballY = W(at::ballY);
  if (ballY >= kw(0x4283, 4) && ballY <= kw(0x428e, 4)) {
    B(0x2f0a) = 0xff;
  } else if (W(0x2f02) >= (high() ? kw(0x42ae, 1) : kw(0x42a1, 1))) {
    B(0x2f0b) = 0xff;
  }
  if (ballY >= static_cast<u16>(W(0x2f02) + (high() ? 0x9e : 0x67))) {
    if (B(0x2f06) != 0xff) B(0x2f09) = 0xff;
    else B(0x2f06) = 0;
    physicsSteps();
    B(0x2f07) = 0;
  } else if (B(0x2f06) != 0xff) {
    B(0x2f06) = 0xff;
    B(0x2f07) = 0xff;
    B(0x2f09) = 0xff;
    physicsSteps();
  } else {
    physicsSteps();
    B(0x2f07) = 0xff;
  }
  afterSteps();
  B(0x23aa) = 0;
  if (B(0x23ab) != 0) {
    CB(0x444b) = 0xff;
    return;
  }
  CB(0x444b) = 0xff;
  B(0x23ab) = 0xff;
  todo(0x2ab1);  // the table's own
  runRules();
  call(W(at::plungerRoutine));
  if (B(0x36f2) != 0) {
    const u8 al = CB(0x444a);
    CB(0x444a) = 0xff;
    if (al != 0xff) {
      displayStep();
      CB(0x444a) = 0;
    }
  }
  flushColours();
  B(0x23ab) = 0;
}

// ---------------------------------------------------------------------------------------
// cs:55db, the callback part way down the frame
// ---------------------------------------------------------------------------------------
void Engine::midFrameCallback() {
  B(0x354d) = 0xff;
  B(0x354e) = 0xff;
  if (B(0x230e) != 0) return;
  if (B(0x2f29) != 0xff) return;
  B(0x36f2) = 0xff;
  if (B(0x3713) == 0xff) return attractMidFrame();
  if (B(at::inMidFrame) != 0) return;
  if (CB(0x444b) != 0xff) return;
  scroll();
  if (B(0x23aa) != 0) return;
  B(at::inMidFrame) = 0xff;
  physicsSteps();
  if (B(0x2f07) == 0xff) B(0x2f09) = 0;
  if (const u8 al = B(0x3714); al != 0) {  // a game was asked for: with this many players
    B(0x3714) = 0;
    B(0x3716) = static_cast<u8>(al - 0x3a);
    B(0x1d9b) = static_cast<u8>(al - 0x3a + 0x37);
    startScript(A(0x1aea));
    effect(0xc65);
    B(0x33e3) = 0;
    addTimer(F(0x61e9));
  }
  CB(0x444b) = 0;
  B(at::inMidFrame) = 0;
}

/// cs:6011: the frame while no game is played.
void Engine::attractFrame() {
  attractLights();
  displayFlash();
  if (B(0x36f2) != 0) {
    const u8 al = CB(0x444a);
    CB(0x444a) = 0xff;
    if (al != 0xff) {
      displayStep();
      CB(0x444a) = 0;
    }
  }
  flushColours();
}

/// cs:60c5: the screen drifts up and down the table, and a game starts when one is asked for.
void Engine::attractMidFrame() {
  const i16 bottom = high() ? 0x103 : 0x171;
  if (W(0x3383).s() >= bottom) W(0x3718) = 0xffff;
  if (W(0x3383).s() <= 0) W(0x3718) = 1;
  W(0x3383) += W(0x3718);
  scroll();
  if (const u8 al = B(0x3714); al != 0) {
    B(0x3714) = 0;
    B(0x3716) = static_cast<u8>(al - 0x3a);
    B(0x1d9b) = static_cast<u8>(al - 0x3a + 0x37);
    startScript(A(0x1adc));
    effect(0xc65);
    W(0x3383) = 0xffff;
    B(0x3713) = 0;  // cs:5ff9
    newGame();
    todo(0x000b);
    todo(0x0202);
    todo(0x0bca);
    B(0x33e3) = 0;
    addTimer(F(0x61e9));
  }
}

/// cs:4018: where the screen looks. It follows the ball, more or less quickly as the options
/// say, unless something has said where to look.
void Engine::scroll() {
  i16 row = static_cast<i16>(W(at::ballY) - (high() ? 0x82 : 0x4b));
  const i16 last = high() ? 0x103 : 0x171;
  if (row < 0) row = 0;
  else if (static_cast<u16>(row) >= static_cast<u16>(last)) row = last;
  if (W(0x3383) != 0xffff) {
    row = static_cast<i16>(W(0x3383) + 0x21);
    W(0x2f04) = static_cast<u16>(row << 4);
    W(0x2f02) = static_cast<u16>(row - 0x21);
    screenRow_ = static_cast<u16>(row);
    return;
  }
  if (W(0x3385) != 0xffff) row = W(0x3385).s();
  row = static_cast<i16>(row + 0x21);
  i16 step = static_cast<i16>((row - (W(0x2f04).s() >> 4)) * W(0x23ac).s());
  W(0x2f04) += static_cast<u16>(step >> 2);
  i16 off = static_cast<i16>(row - (W(0x2f04).s() >> 4));
  if (off >= 0) {
    off = static_cast<i16>(off - (high() ? 0xaa : 0x73));
    if (off >= 0) W(0x2f04) += static_cast<u16>(off << 4);
  } else {
    off = static_cast<i16>(off + (high() ? 0x82 : 0x4b));
    if (off <= 0) W(0x2f04) += static_cast<u16>(off << 4);
  }
  const i16 shown = static_cast<i16>((W(0x2f04).s() >> 4) + W(at::nudgeLift).s());
  screenRow_ = static_cast<u16>(shown);
  W(0x2f02) = static_cast<u16>(shown - 0x21);
}

// ---------------------------------------------------------------------------------------
// cs:35fd, the program's own loop
// ---------------------------------------------------------------------------------------
void Engine::mainLoop() {
  if (pause_ != Pause::No) return paused();  // the original waits inside the pause for a key
  if (CB(0x3475) != 0) {
    u8& key = B(at::lastKey);
    if (key == 0x43) { key = 0xff; W(0x23ac) = 0x09; }  // F9, F10: how fast the screen follows
    if (key == 0x44) { key = 0xff; W(0x23ac) = 0x0b; }
    if (B(0x3553) == 0x0b) { B(0x3553) = 0; W(0x23ac) = 0x14; }
    if (B(0x3553) == 0x0c) { B(0x3553) = 0; W(0x23ac) = 0x28; }
    if (B(0x3713) == 0xff) {  // no game
      playersKey();
      typeCheat();
      if (key == 0x01) {  // escape: asks whether to quit (cs:3196)
        key = 0xff;
        if (CB(0x3195) != 0xff) {
          CB(0x3475) = 0;
          CB(0x3195) = 0xff;
          startScript(A(0x4414));
        }
      }
    } else if (B(0x33ce) != 0xff) {
      bool keys = true;
      if (B(0x338a) == 0xff) {
        if (key == 0x01) {  // escape while the ball waits on the plunger: the game is given up (cs:32b1)
          key = 0xff;
          placeBall(0x12c, 0x23a);
          CB(0x3475) = 0xff;
          addTimer(F(0x0b73));
          call(F(0x01cd));
          B(at::lastKey) = 0xff;
          displaySteady();
          startScript(A(0x19d4));
        }
        if (CB(0x3475) == 0) keys = false;
        else playersKey();
      }
      if (keys) {
        if (key == 0x39) { key = 0xff; nudgeKey(); }
        if (key == 0x32) { key = 0xff; musicKey(); }
        if (key == 0x19) { key = 0xff; B(0x230f) = 0xff; }
        pauseKey();
        if (pause_ != Pause::No) return;  // the rest of this turn waits with it
      }
    }
  }
  if (B(at::lastKey) == 0x01) B(at::lastKey) = 0xff;
  ++W(at::loopCounter);
  if (B(at::ballHidden) != 0) {  // the spin a ball will be served with is drawn afresh all the while
    u16 spin = W(at::loopCounter) & 0x3ff;
    if (spin & 1) spin = static_cast<u16>(-spin);
    W(at::spin) = spin;
  }
  if (CB(0x3732) == 0xff) {
    exited_ = true;
    return;
  }
  // The driver is polled; the silent one calls the music's callback every time it is.
  if (pollCallsMusic) musicCallback(8);
}

/// cs:33f9: F1 to F8 start a game for that many players; enter adds one.
void Engine::playersKey() {
  u8 al = B(at::lastKey);
  if (al >= 0x3b && al <= 0x42) {
    B(at::lastKey) = 0xff;
    if (B(0x33e3) == 0) return;
    B(0x33e3) = 0;
    B(0x3714) = al;
    B(0x3715) = al;
    B(0x3717) = 0;
  }
  if (al != 0x1c) return;
  B(at::lastKey) = 0xff;
  if (B(0x3716) >= 8) return;
  if (B(0x33e3) == 0) return;
  if (B(0x3717) != 0) {
    B(0x3716) = 0;
    B(0x3717) = 0;
  }
  ++B(0x3716);
  al = static_cast<u8>(B(0x3716) + 0x3a);
  B(0x33e3) = 0;
  B(0x3714) = al;
  B(0x3715) = al;
}

/// cs:3549: letters typed while no game is played are matched against the cheats' words.
void Engine::typeCheat() {
  u8 al = B(at::lastKey);
  if (al == 0xff) return;
  al = B(0x3654, al);  // scancode to letter
  if (al == 0) return;
  B(0x3729) = al;
  B(at::lastKey) = 0xff;
  B(0x372f, W(0x372d)) = al;
  ++W(0x372d);
  if (W(0x372d) > 0x0c) {
    W(0x372d) = 0;
    for (u16 i = 0; i < 0x0c; ++i) B(0x372f, i) = 0x20;
    return;
  }
  u16 s = A(0x373d);
  B(0x372b) = 0;
  for (;;) {
    const u16 handler = nativeW(s);
    s = static_cast<u16>(s + 2);
    if (handler == 0xff) break;
    u16 typed = 0;
    bool skip = false;
    for (;;) {
      const u8 c = nativeB(s++);
      if (c == 0x24) {  // the whole word: its routine
        W(0x372d) = 0;
        call(handler);
        return;
      }
      if (typed >= W(0x372d)) {  // so far so good, with more to come
        B(0x372b) = 0xff;
        skip = true;
        break;
      }
      ++typed;
      if (c != B(0x372f, static_cast<u16>(typed - 1))) {
        skip = true;
        break;
      }
    }
    if (skip)
      while (nativeB(s++) != 0x24) {}
  }
  if (B(0x372b) == 0xff) return;
  W(0x372d) = 1;  // no word begins like this: start again from this letter
  B(0x372f) = B(0x3729);
  for (u16 i = 1; i < 0x0c; ++i) B(0x372f, i) = 0x20;
}

/// cs:3478: a press of the space bar shakes the table.
void Engine::nudgeKey() {
  if (B(0x372a) == 0xff) return;
  if (B(0x33e0) != 0xff && B(at::tilted) != 0xff && B(at::ballLost) != 0xff && CB(0x3477) != 0xff) {
    W(at::tiltCounter) += 0x3c;
    CB(0x3477) = 0xff;
    if (W(at::tiltCounter) > 0x78) {
      B(at::tilted) = 0xff;
      todo(0x0fa0);
    } else if (W(at::tiltCounter) > 0x3c) {
      todo(0x0fc5);
    }
  }
  B(at::lastKey) = 0xff;
}

/// cs:34e3: M turns the music off and on.
void Engine::musicKey() {
  B(0x231a) ^= 0xff;
  if (B(0x231a) == 0) {
    B(0xc6f) = 0;
    B(0xc72) = kb(0x34f2, 1);
    B(0x230a) = kb(0x34f7, 4);
    if (B(0x33e0) == 0xff) B(0x230a) = 0;
    if (static_cast<i8>(B(0x230d)) < 1) B(0x230d) = 1;
  } else {
    const u8 silence = kb(0x3522, 1);  // the place in the table's music where nothing plays
    B(0xc6f) = silence;
    B(0xc72) = silence;
    if (B(0x230a) != silence) addTimer(F(0x353b));
  }
}

/// cs:31cd: P pauses. The callbacks stop doing anything, the music stops, and the display
/// says so over what it showed, which is kept to put back.
void Engine::pauseKey() {
  if (B(0x230f) == 0) return;
  B(0x2f29) = 0;
  sound->stop();
  saveDisplay();
  B(at::lastKey) = 0xff;
  pause_ = Pause::AnyKey;
}

void Engine::paused() {
  const u8 key = B(at::lastKey);
  if (key == 0xff) return;
  if (pause_ == Pause::AnyKey && key == 0x01) {  // escape: leave the table?
    message(A(0x4432), 0x150);
    B(at::lastKey) = 0xff;
    pause_ = Pause::YesOrNo;
    return;
  }
  if (pause_ == Pause::YesOrNo && B(0x3654, key) == 0x59) {  // yes (cs:323a)
    B(at::ballHidden) = 0xff;
    restoreDisplay();
    B(0x230f) = 0;
    B(0x231a) ^= 0xff;
    CB(0x3a10) = 0xff;
    sound->volume(0);
    B(0x2f29) = 0xff;
    sound->start();
    exited_ = true;
    pause_ = Pause::No;
    return;
  }
  // any other key: on with the game (cs:3203)
  B(0x33f0) = 0x1e;
  B(at::lastKey) = 0xff;
  sound->start();
  restoreDisplay();
  B(0x2f29) = 0xff;
  B(0x230f) = 0;
  pause_ = Pause::No;
  // what the loop does after the pause, which this turn of it has yet to do
  if (B(at::lastKey) == 0x01) B(at::lastKey) = 0xff;
  ++W(at::loopCounter);
  if (B(at::ballHidden) != 0) {
    u16 spin = W(at::loopCounter) & 0x3ff;
    if (spin & 1) spin = static_cast<u16>(-spin);
    W(at::spin) = spin;
  }
  if (pollCallsMusic) musicCallback(8);
}

void Engine::message(u16 text, u16 at) {
  W(0x3700) = 0xa8;
  W(0x3702) = 0x50;
  W(0x3704) = 0x10;
  fillDisplay(0xa8, 0x50, 0x10);
  setFont(0);
  W(0x447b) = 0;
  W(0x447d) = 0;
  drawText(text, at);
}

void Engine::saveDisplay() {
  displayNormal();
  flushColours();
  u16 to = A(0x33fd);
  B(0x33fc) = 0x80;
  for (int plane = 0; plane < 2; ++plane)
    for (u16 row = 0; row < 0x10; ++row) {
      nativeB(to) = 0;
      for (u16 x = 0; x < 0x50; ++x) {
        if (dot(plane, static_cast<u16>(0xa8 + row * 0xa8 + x)) == lit()) nativeB(to) |= B(0x33fc);
        const bool wrapped = B(0x33fc) & 1;
        B(0x33fc) = static_cast<u8>((B(0x33fc) >> 1) | (wrapped ? 0x80 : 0));
        if (wrapped) nativeB(++to) = 0;
      }
    }
  W(0x33fa) = to;
  W(0x36fc) = A(0x353e);
  W(0x36fe) = 0x162;
  message(A(0x353e), 0x162);
  displayNormal();
  flushColours();
}

void Engine::restoreDisplay() {
  u16 from = A(0x33fd);
  B(0x33fc) = 0x80;
  for (int plane = 0; plane < 2; ++plane)
    for (u16 row = 0; row < 0x10; ++row)
      for (u16 x = 0; x < 0x50; ++x) {
        dot(plane, static_cast<u16>(0xa8 + row * 0xa8 + x)) = (nativeB(from) & B(0x33fc)) ? lit() : unlit();
        const bool wrapped = B(0x33fc) & 1;
        B(0x33fc) = static_cast<u8>((B(0x33fc) >> 1) | (wrapped ? 0x80 : 0));
        if (wrapped) ++from;
      }
  W(0x33fa) = from;
}

/// cs:3a6a: called by the driver when the music comes to a jump (and by the silent driver
/// whenever it is polled). What it is given and gives back in AL is the place to go on from.
u8 Engine::musicCallback(u8 al) {
  const u8 left = --B(0x230d);
  if (left == 0) {
    B(0x3389) = B(0x230c);
    B(0x230d) = 0;
    al = B(0x230a);
    B(0x338e) = 0xff;
    B(0x338f) = 0xff;
  } else if (left == 0xff || left == 0x7f) {  // below zero, as the original's signed test has it
    B(0x230d) = 0;
  }
  if (B(0x231a) != 0 && al <= kb(0x3aac, 1)) al = kb(0x3ab3, 1);
  return al;
}

// ---------------------------------------------------------------------------------------
// timers
// ---------------------------------------------------------------------------------------
void Engine::addTimer(u16 native) {
  // The original looks for a free slot with BX and leaves it there. A step that goes on to
  // use BX as its place in the script, without having kept it, goes astray in the original
  // too (Party Land's cs:0b5b does), so BX is left the same way here.
  for (bx = A(0x331b); bx != A(0x337f); bx = static_cast<u16>(bx + 2)) {
    if (nativeW(bx) != F(0x69fc)) continue;
    ++W(0x337f);
    nativeW(bx) = native;
    return;
  }
  exited_ = true;  // the original gives up when all fifty are taken
}

void Engine::runTimers() {
  u16 slot = A(0x331b);
  for (int i = 0, slots = kw(0x5b2a, 1); i < slots; ++i, slot = static_cast<u16>(slot + 2)) {
    W(0x3381) = slot;
    bx = slot;  // as the original has it, which matters to a routine that takes BX for its place in a script
    call(nativeW(slot));
  }
}

void Engine::endTimer() {
  nativeW(W(0x3381)) = F(0x69fc);
  --W(0x337f);
}

bool Engine::countTo(u16 nativeCounter, u16 limit) {
  if (nativeW(nativeCounter) == limit) {
    nativeW(nativeCounter) = 0;
    return true;
  }
  ++nativeW(nativeCounter);
  return false;
}

}  // namespace encore
