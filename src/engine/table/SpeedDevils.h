#pragma once
// Speed Devils' own rules (TABLE2.PRG, cs:0000 to cs:2799), on the engine the tables share.
// Addresses here are this table's own. Routines are named by where they are in the program
// until what they are for is clearer.
#include <initializer_list>

#include "engine/table/Flow.h"

namespace encore {

class SpeedDevils : public Flow {
 public:
  explicit SpeedDevils(ByteView prg);

 private:
  u8& b(u16 a) { return nativeB(a); }
  Word w(u16 a) { return nativeW(a); }
  void sfx(u16 record) { sound->effect(b(record), b(static_cast<u16>(record + 1)), 0, static_cast<u8>(b(static_cast<u16>(record + 3)) + 1)); }
  /// Where in a blink's period a light begins that is to be out of step with its neighbour.
  static u8 later(u8 start, u8 half) {
    start = static_cast<u8>(start + half);
    return start >= half * 2 ? static_cast<u8>(start - half * 2) : start;
  }
  /// A row of lights shown as a row of flags has them: lit or out, and not blinking.
  void row(u16 flags, u16 count, u8 firstLight);
  void scored();
  void addBonus(u16 amount);
  void lights(std::initializer_list<u8> which, void (SpeedDevils::*what)(u8));
  void stop(u8 light) { stopBlink(light); }
  void on(u8 light) { lightOn(light); }
  void off(u8 light) { lightOff(light); }
  void clear(u8 light) { clearLight(light); }
  void flash(u8 light, u8 half) { blink(light, 0, half); }
  void hold();  ///< the ball kept out of sight in the pit lane

  void savePlayer(u16 player) override;   // cs:0f9e
  void restorePlayer() override;          // cs:0c84
  void clearBall() override;              // cs:004c
  bool extraBallOwed() override { return b(0x0071) != 0; }
  void matchPace() override { nativeCW(0x07e0) = nativeCW(0x07e2) = high() ? 0x0d : 0x0b; }
  u16 matchLength() override { return kw(0x0866, 1); }
  void gameOverLights() override {}
  u16 matchRepeated(u16 digit) override { return digit; }
  void matchWon() override { b(0x0070) = 0xff; }
  bool restoresAtTurn() override { return false; }
  void ballShown() override { b(0x2062) = b(0x209e); }
  void playerShown() override { b(0x2058) = b(0x2097); }
  bool redrawNameWhileWaiting() override { return false; }
  void sameBallAgain() override;

  void bindRules();
  void bindSteps();
  void at0217();
  void at0225() { addScore(0x096a, 0x0976); }
  bool leftLanes();    // cs:1534
  bool rightLanes();   // cs:15b9
  bool at1638();
  void at16f0();
  void at1748();
  void at17ad();
  void at17f1();
  void at186a();
  void at18db();
  void at18ed();
  void at1920();
  void at1b4f();
  bool at2031();
  void at219b();
  void at2201();
  void at226b();
  void at2281();
  void at24de();
  void at24f8();
  void everyFrame();   // cs:241a
  void threeDigits(u16 value, u16 at, bool blankHundreds);
};

}  // namespace encore
