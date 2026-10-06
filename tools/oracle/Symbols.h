#pragma once
// Where the referee's tools find things in each table program's memory. The four programs hold
// the same engine at different addresses; these were found once in Party Land's listing and
// carried to the others by lining the programs up (re/align.py).
#include "Cpu.h"

namespace oracle {

struct Symbols {
  u16 data;                     ///< the data segment, as the program's listing names it
  u16 spin, x, y, xFixed, yFixed, vx, vy;  ///< the ball
  u16 layer;                    ///< 0 on the playfield, 0xff on the ramps
  u16 hidden;                   ///< 0xff while there is no ball in play
  u16 keys;                     ///< bit 0 right flipper, bit 1 left
  u16 loops;                    ///< counts the program's own loop: its source of chance
};

inline constexpr Symbols kSymbols[4] = {
    {0x19b5, 0x2ede, 0x2ee0, 0x2ee2, 0x2ee4, 0x2ee8, 0x2eec, 0x2eee, 0x331a, 0x2f2a, 0x2f28, 0x33ed},
    {0x18cf, 0x2cee, 0x2cf0, 0x2cf2, 0x2cf4, 0x2cf8, 0x2cfc, 0x2cfe, 0x331e, 0x2d3a, 0x2d38, 0x33b5},
    {0x1896, 0x29a0, 0x29a2, 0x29a4, 0x29a6, 0x29aa, 0x29ae, 0x29b0, 0x2be8, 0x29ec, 0x29ea, 0x2c7f},
    {0x164d, 0x336a, 0x336c, 0x336e, 0x3370, 0x3374, 0x3378, 0x337a, 0x386e, 0x33b6, 0x33b4, 0x3905},
};

}  // namespace oracle
