#include "sim/Flipper.h"

#include "sim/Mask.h"

namespace pfr {

Flipper Flipper::decode(ByteView d, std::size_t o) {
  auto w = [&](std::size_t at) { return static_cast<i16>(rd16le(d, o + at)); };
  Flipper f;
  f.keySide = d[o];
  if (f.keySide == 0) return f;
  f.originX = w(0x02);
  f.originY = w(0x04);
  f.frameWidth = w(0x06) * 2 * 8;  // words per row -> pixels
  f.frameRows = w(0x08);
  f.box = {w(0x0a), w(0x0e), w(0x0c) - w(0x0a), w(0x10) - w(0x0e)};
  f.pivotX = w(0x12);
  f.pivotY = w(0x14);
  f.vertical = w(0x16) != 0;
  f.maxFrame = w(0x20);
  f.positionAtMax = w(0x22);
  f.stepUp = w(0x24);
  f.stepDown = w(0x26);
  f.initialUp = w(0x28);
  return f;
}

void Flipper::update(bool held, bool enabled) {
  if (!valid()) return;
  if (held && enabled) {
    omega += stepUp;
    // A press snaps straight to the initial speed, then keeps accelerating while held.
    if (omega > initialUp) omega = initialUp;
  } else {
    omega += stepDown;
  }
  position -= omega;
  if (position <= 0) {
    position = 0;
    omega = 0;
    frame = 0;
    return;
  }
  frame = position / kPositionPerFrame;
  if (frame >= maxFrame) {
    frame = maxFrame;
    position = positionAtMax;
    omega = 0;
  }
}

}  // namespace pfr
