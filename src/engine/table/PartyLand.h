#pragma once
// Party Land's own rules (TABLE1.PRG, cs:0000 to cs:2f9a), on the engine the tables share.
#include "engine/table/Engine.h"

namespace encore {

class PartyLand : public Engine {
 public:
  explicit PartyLand(ByteView prg);

 private:
  void newGameTable();    // cs:000b
  void clearScores();     // cs:0054
  void clearBall();       // cs:00aa
  void savePlayer(u16 player);     // cs:0e83: `player` is its place in the players' table
  void restorePlayer();   // cs:0d48
  void serve();           // cs:0bca
  void startBall();       // cs:0c79
  void everyFrame();      // cs:2ab1
  void drained();         // cs:0215
  void bindRules();       // PartyLandRules.cpp
  void bindSteps();

  // (named by where they are in the program until what they are is clearer)
  void hole() { placeBall(0x0f, 0x2f); }   ///< the ball put away, out of sight
  void eject();           // cs:1126
  bool allFive();         // cs:1249
  void fiveLit();         // cs:126c
  void at1287();
  void at1302();
  void at12ea(bool carry);
  void hitScore();        // cs:152e
  bool at156d();
  bool at19c7();
  void at1b19();
  void at1c82();
  void at1ddb();
  void at1efb();
  bool at232a();
  bool at2364();
  bool at23a7();
  void at2420() { addScore(0x010c, 0x0118); }
  void at26b0();
  bool laneScore();       // cs:2842
  void at28f5();
  void at2a94();
  void at2b9d();
  void at2bb7();
  void at2540();
  void countdownTick();   // cs:2ddc
  /// The bonus goes up by an amount, once for each of its multiplier (cs:154e and its like).
  void addBonus(u16 amount);
  /// The usual end of a score: the display goes back to showing it.
  void scored();
};

}  // namespace encore
