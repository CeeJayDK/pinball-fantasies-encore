#pragma once
// Bumpers and kickers: the elements that throw the ball back and score on their own,
// without going through the table's trigger handlers.
#include <vector>

#include "engine/data/TableSound.h"
#include "engine/game/Score.h"

namespace encore {

struct Bumper {
  Rect rect;
  bool kicker = false;  ///< a kicker is made of rubber, a bumper of plastic
  Sfx sfx;
  Score score;
};

std::vector<Bumper> extractBumpers(ByteView dataSegment, int tableIndex);

}  // namespace encore
