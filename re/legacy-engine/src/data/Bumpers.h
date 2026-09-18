#pragma once
// Bumpers and kickers: the elements that throw the ball back and score on their own,
// without going through the table's trigger handlers.
#include <vector>

#include "data/TableSound.h"
#include "game/Score.h"

namespace pfr {

struct Bumper {
  Rect rect;
  bool kicker = false;  ///< a kicker is made of rubber, a bumper of plastic
  Sfx sfx;
  Score score;
};

std::vector<Bumper> extractBumpers(ByteView dataSegment, int tableIndex);

}  // namespace pfr
