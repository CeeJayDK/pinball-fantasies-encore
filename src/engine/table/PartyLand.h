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
};

}  // namespace encore
