#pragma once
// What the four tables do alike between balls and at the end of a game: the ball served, the
// bonus counted, the next player or ball, the best scores, the match. In each program this is
// among the table's own code (Party Land: cs:0202 to cs:0d47), nearly the same in all four;
// where a table does it differently it overrides the piece. Written against Party Land's
// addresses like the rest of the engine.
#include "engine/table/Engine.h"

namespace encore {

class Flow : public Engine {
 public:
  Flow(ByteView prg, int table);

 protected:
  // --- what each table has of its own
  /// Keeps what the player whose record is at `player` (an offset into the players' table)
  /// carries from ball to ball, and brings the current player's back.
  virtual void savePlayer(u16 player) = 0;
  virtual void restorePlayer() = 0;
  /// Everything about the ball in play as a ball begins (Party Land: cs:00aa).
  virtual void clearBall() = 0;
  /// Whether the player has another ball to come without it counting (Party Land: the light).
  virtual bool extraBallOwed() { return B(0x35c4) == 0xff; }
  /// The match's pace: how many frames each digit stands (Party Land: cs:0883, cs:08ab).
  virtual void matchPace() { CW(0x089d) = high() ? 0x0b : 0x09; }
  virtual void ballShown() {}    ///< the ball's number was written for the display
  virtual void playerShown() {}  ///< and the player's
  virtual bool redrawNameWhileWaiting() { return true; }
  virtual void beforeSameBallAgain() { --B(0x00ce); }  ///< Party Land: cs:0b36

  void serve();           // cs:0bca
  void startBall();       // cs:0c79
  u16 playerSize() const { return kw(0x05d4, 1); }
  u16 currentPlayer() { return static_cast<u16>((B(0x371a) - 1) * playerSize()); }
  /// The step after this one is run at once (cs:0b1d).
  void chain();
  void goTo(u16 nativeScript);  ///< go on from another script, at once

 private:
  void bindFlow();
};

}  // namespace encore
