#include "engine/sim/Ball.h"

namespace encore {

void Ball::place(int px, int py) {
  x = static_cast<i16>(px);
  y = static_cast<i16>(py);
  xFixed = px * 1024;
  yFixed = py * 1024;
  lost = false;
}

void Ball::setVelocity(int nvx, int nvy) {
  vx = static_cast<i16>(nvx);
  vy = static_cast<i16>(nvy);
}

}  // namespace encore
