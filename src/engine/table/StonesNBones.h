#pragma once
// Stones 'n Bones' own rules (TABLE4.PRG, cs:0000 to cs:3746), on the engine the tables
// share. Addresses here are this table's own. Routines are named by where they are in the
// program until what they are for is clearer.
#include <initializer_list>

#include "engine/table/Flow.h"

namespace encore {

class StonesNBones : public Flow {
 public:
  explicit StonesNBones(ByteView prg);

 private:
  u8& b(u16 a) { return nativeB(a); }
  Word w(u16 a) { return nativeW(a); }
  void sfx(u16 record, u8 how = 0) { sound->effect(b(record), b(static_cast<u16>(record + 1)), how, static_cast<u8>(b(static_cast<u16>(record + 3)) + 1)); }
  void wait1() {
    w(0x3901) = 1;
    w(0x38ff) = 0x5a31;
  }
  void copy12(u16 to, u16 from) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(to + i)) = b(static_cast<u16>(from + i));
  }
  void zero12(u16 at) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(at + i)) = 0;
  }
  bool isLit(u8 light) { return b(static_cast<u16>(0x3b6d + light)) == 0xff; }
  void lights(std::initializer_list<u8> which, void (StonesNBones::*what)(u8));
  void stop(u8 light) { stopBlink(light); }
  void clear(u8 light) { clearLight(light); }
  void flash(u8 light) { blink(light, 0, 2); }
  /// A light put out for good: no longer blinking, and not remembered as lit.
  void out(u8 light) {
    stopBlink(light);
    clearLight(light);
  }
  /// The ball put somewhere, on the playfield or on the ramps, standing still.
  void place(u16 x, u16 y, u8 layer) {
    placeBall(x, y);
    b(0x386e) = layer;
  }
  /// A score that is not an award's: the score itself, and what every score does (cs:14b1).
  void scored(u16 amount);
  /// The bonus goes up by an amount, once for each step of its multiplier (cs:14df).
  void addBonus(u16 amount);
  /// A row of three lights shown as their flags have them: lit or out, not blinking (cs:0fad).
  void row(u16 flags, u8 firstLight);

  // --- pieces of the collision masks that come and go: a record names two shapes and where
  // they go (cs:6721 to cs:6776)
  void shape(u16 segment, u16 record, bool second);
  void shapeA(u16 record) { shape(0x345b, record, false); }
  void shapeB(u16 record) { shape(0x345b, record, true); }
  void rampA(u16 record) { shape(0x3f9b, record, false); }
  void rampB(u16 record) { shape(0x3f9b, record, true); }

  void savePlayer(u16 player) override;   // cs:0e77
  void restorePlayer() override;          // cs:0d01
  void clearBall() override;              // cs:0056
  bool extraBallOwed() override { return b(0x0226) != 0; }
  void matchWon() override { b(0x0206) = 0xff; }
  void gameOverLights() override {}
  bool clearsScoreAtEnd() override { return false; }
  bool restoresAtTurn() override { return false; }
  void afterRestore() override {}
  void serveMusic() override { music(0x08f2); }
  void ballShown() override { b(0x26be) = b(0x26fa); }
  void playerShown() override { b(0x26b4) = b(0x26f3); }
  bool redrawNameWhileWaiting() override { return false; }
  void sameBallAgain() override;

  void bindMatch();
  void bindRules();
  void bindSteps();
  void everyFrame();       // cs:3189
  void at1c68();
  void at21c2();
  void at2506();
  void at149a();
  void at14f8();
  void at1556();
  void at1708();
  void at1795();
  void at1af4();
  bool at2060();
  void at2162();
  void at2a9c();
  void at2d9e();
  void at2f85();
  void at2fbe();
  void digits(u16 value, u16 at);  // cs:2f5d
  /// cs:1d37 and its like: light 7 blinks, if it does not already.
  void light7();
  void at34f8() { addScore(0x0299, 0x02b1); }
  void at3504() { addScore(0x02bd, 0x02d5); }
  void at3510() { addScore(0x02e1, 0x02f9); }
  void at351c() { addScore(0x0275, 0x0281); }
  void at3528();
  void at3542();
  void scrollPicture(u16 row);  // cs:36c0
};

}  // namespace encore
