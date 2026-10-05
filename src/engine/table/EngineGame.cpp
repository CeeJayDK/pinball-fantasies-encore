// A game's comings and goings: starting one, the ball's beginning, scores.
#include "engine/table/Engine.h"

namespace encore {

/// cs:3881
void Engine::newGame() {
  CB(0x3475) = 0xff;
  B(0x372c) = 0;
  B(0x33dc) = 1;  // the ball
  B(0x228f) = static_cast<u8>(B(0x33dc) + 0x37);
  B(0x371a) = 1;  // the player
  B(0x2288) = static_cast<u8>(B(0x371a) + 0x37);
  B(0x33ef) = 0xff;
  // The original adds up a stretch of its own code here, to see that nobody has changed it,
  // and only then lets players be added and the high scores stand.
  u8 sum = 0;
  for (u16 a = F(0x3030); a != F(0x305a); ++a) sum = static_cast<u8>(sum + cs_[a]);
  if (static_cast<u8>(sum - 2) == cs_[static_cast<u16>(F(0x38c8) + 2)]) {
    B(0x33e3) = 0xff;
    B(0x33ef) = 0;
    B(0x33f9) = 0xff;
  }
  B(0x33f1) = 0;
  B(0x33f2) = 0xff;
}

/// cs:37ea
void Engine::beginBall() {
  W(0x3316) = 0;
  B(0x338a) = 0xff;
  B(0x338b) = 0;
  W(0x338c) = F(0x69fc);
  B(0x338e) = 0xff;
  B(0x338f) = 0xff;
  B(0x3398) = 0;
  B(0x33ce) = 0;
  B(0x33e2) = 0;
  B(0x33f7) = 0;
  B(0x33f9) = 0xff;
  stopBlinks();
  for (u16 i = 0; i < 0x32; ++i) W(0x331b, static_cast<u16>(i * 2)) = F(0x69fc);  // cs:3870
  for (u16 i = 0; i < 0x32; ++i) W(0x35ca, static_cast<u16>(i * 2)) = 0;          // cs:37dd
  if (B(0x00d0) == 0xff) return;
  B(0x3396) = 0;
  displaySteady();
  if (B(0x33f2) == 0xff) {
    B(0x33f2) = 0;
    return;
  }
  for (u16 i = 0; i < 12; ++i) B(0x45da, i) = 0x12;
  W(0x45e6) = 0;
  startScript(A(0x1acc));
}

void Engine::toAttract() {
  CB(0x3195) = 0;
  CB(0x3475) = 0xff;
  B(0x3713) = 0xff;
}

/// cs:5867: every light out, at once and not through the queue.
void Engine::lightsOut() {
  if (B(0x2f2b) != 0xff) return;
  for (u16 light = 1; light <= 0x38; ++light) {
    B(0x3591, light) = 0;
    u16 record = W(0x12bd, static_cast<u16>((light - 1) * 2));
    B(0x354e) = 0;
    std::size_t colour = std::size_t{nativeB(record++)} * 3;
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i, ++colour) dac_[colour % 768] = (nativeB(record++) >> 1) & 0x3f;
  }
}

bool Engine::music(u16 record) {
  if (record == 0) return true;
  const u8 place = nativeB(record), repeats = nativeB(static_cast<u16>(record + 1)), priority = nativeB(static_cast<u16>(record + 2));
  if (priority < B(0x3389)) return false;
  B(0x3389) = priority;
  const u8 before = B(0x230d);
  B(0x230d) = repeats;
  const u8 answer = sound->jump(place);
  B(0x230d) = repeats;
  B(0x338e) = 0;
  B(0x338f) = 0;
  if (static_cast<i8>(before) > 0) return true;
  if (answer == 0x3e) return true;
  B(0x230a) = answer;
  return true;
}

void Engine::addScore(u16 to, u16 amount) {
  u8 carry = 0;
  for (int i = 11; i >= 0; --i) {
    // as the original's "add, then adjust": a digit over nine carries one up
    u8 sum = static_cast<u8>(nativeB(static_cast<u16>(to + i)) + nativeB(static_cast<u16>(amount + i)) + carry);
    carry = 0;
    if ((sum & 0x0f) > 9) {
      sum = static_cast<u8>(sum + 6);
      carry = 1;
    }
    nativeB(static_cast<u16>(to + i)) = sum & 0x0f;
  }
}

void Engine::placeBall(u16 x, u16 y) {
  W(at::ballX) = x;
  W(at::ballY) = y;
  B(at::layer) = 0;
  const u32 fx = u32{x} * 0x400, fy = u32{y} * 0x400;
  W(at::ballXFixed) = static_cast<u16>(fx);
  W(at::ballXFixed, 2) = static_cast<u16>(fx >> 16);
  W(at::ballYFixed) = static_cast<u16>(fy);
  W(at::ballYFixed, 2) = static_cast<u16>(fy >> 16);
  W(at::ballVy) = 0;
  W(at::ballVx) = 0;
}

void Engine::patchMask(u16 segment, u16 at, u16 shape, u16 width, u16 rows) {
  const u16 native = S(segment);
  for (u16 r = 0; r < rows; ++r, at = static_cast<u16>(at + 0x28))
    for (u16 x = 0; x < width; ++x) {
      const u8 v = nativeB(shape++);
      farB(native, static_cast<u16>(at + x)) = v;
      maskChanged(native, static_cast<u16>(at + x), v);
    }
}

void Engine::bindGame() {
  bind(0x61e9, [this] {  // players may be added again a moment after the last
    if (!countTo(A(0x3616), 0x0f)) return;
    B(0x33e3) = 0xff;
    endTimer();
  });
}

}  // namespace encore
