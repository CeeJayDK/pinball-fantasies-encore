#pragma once
// The table's triggers: the rectangles that tell the game the ball did something.
//
// There are two kinds. A roll trigger fires when the ball's centre enters its rectangle;
// a hit trigger fires when the ball collides with something solid inside one. Each carries
// the address of the original's handler for it, which is what identifies the element.
#include <vector>

#include "core/Types.h"

namespace encore {

struct TriggerArea {
  Rect rect;
  u16 handler = 0;   ///< address of the original's routine, used to identify the element
};

struct TableTriggers {
  std::vector<TriggerArea> hit;             ///< checked on collision, both layers
  std::vector<TriggerArea> rollGround;
  std::vector<TriggerArea> rollOverhead;
  std::vector<TriggerArea> rollGroundTilted;    ///< the lists used once the table has tilted
  std::vector<TriggerArea> rollOverheadTilted;

  const std::vector<TriggerArea>& roll(bool upper, bool tilted) const {
    if (tilted) return upper ? rollOverheadTilted : rollGroundTilted;
    return upper ? rollOverhead : rollGround;
  }
};

TableTriggers extractTriggers(ByteView dataSegment, int tableIndex);

}  // namespace encore
