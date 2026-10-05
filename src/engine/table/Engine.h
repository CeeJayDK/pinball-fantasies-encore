#pragma once
// The engine the four tables share, written again routine by routine from Party Land's
// program (docs/own-engine.md). A comment "cs:1234" is where the routine is in TABLE1.PRG.
#include <array>

#include "engine/table/Program.h"

namespace encore {

/// What a table asks of the sound driver (the original's int 66h). The default is the game's
/// own silent driver, NOSOUND.SDR: it plays nothing and answers 0.
class SoundDriver {
 public:
  virtual ~SoundDriver() = default;
  /// Function 0x11: a sample of the module (1-31) at a note (1-36), on a channel (1-4).
  virtual void effect(u8 sample, u8 note, u8 volume, u8 channel) { (void)sample, (void)note, (void)volume, (void)channel; }
  /// Function 0x10: go to a place in the song at the next tick.
  virtual u8 jump(u16 position) { (void)position; return 0; }
  /// Function 0x06: the volume, 0x400 for all of it.
  virtual void volume(u16 level) { (void)level; }
  /// Function 0x0f: stop the music.
  virtual void stop() {}
  /// Function 0x04: start it.
  virtual u8 start() { return 0; }
};

class Engine : public Program {
 public:
  Engine(ByteView prg, int table);

  /// A key goes down or up, as the keyboard says it: a scancode, bit 7 set for up
  /// (cs:3e44, the table's keyboard interrupt).
  void key(u8 scancode);
  /// One video frame, as the driver's two callbacks run it (cs:41c6, cs:55db), then the
  /// program's own loop (cs:35fd) `loopsPerFrame` times.
  void frame();
  int loopsPerFrame = 8;
  /// As the silent driver: the music's callback is called every time the driver is polled.
  bool pollCallsMusic = true;
  /// The program asked to end.
  bool exited() const { return exited_; }
  /// The row of the picture at the top of the screen.
  u16 screenRow() const { return screenRow_; }

  /// The video card's colours as the table has set them: 256 of red, green, blue, 0-63.
  const std::array<u8, 768>& colours() const { return dac_; }
  std::array<u8, 768>& colours() { return dac_; }

  SoundDriver* sound = &silent_;

 protected:
  // --- the frame (EngineFrame.cpp)
  void frameCallback();
  void midFrameCallback();
  void mainLoop();
  void attractFrame();      // cs:6011
  void attractMidFrame();   // cs:60c5
  void scroll();            // cs:4018
  u8 musicCallback(u8 al);  // cs:3a6a
  void keyExtended(u8 al);  // cs:3f8a
  void typeCheat();         // cs:3549
  void playersKey();        // cs:33f9
  void pauseKey();          // cs:31cd
  void nudgeKey();          // cs:3478
  void musicKey();          // cs:34e3

  // --- the ball, and what follows its sub-steps (EnginePhysics.cpp)
  void physicsSteps();      // cs:87c0
  void afterSteps();        // cs:59aa
  void runRules();          // cs:5989

  // --- timers: up to 50 routines run once a frame (cs:5b0b, cs:5b2a, cs:576a)
  void addTimer(u16 native);
  void runTimers();
  void endTimer();
  /// cs:5777: counts the word at `counter` up to `limit`; true, and back to 0, when there.
  bool countTo(u16 nativeCounter, u16 limit);

  // --- lights, which are colours of the picture (EngineLights.cpp)
  void queueColours(u16 nativeRecord, bool half);
  void lightOn(u8 light);      // cs:5728
  void lightOff(u8 light);     // cs:5747
  void setLight(u8 light);     // cs:5788
  void clearLight(u8 light);   // cs:5795
  void blink(u8 light, u8 start, u8 halfPeriod);  // cs:57a2
  void stopBlink(u8 light);    // cs:57c7
  void stopBlinks();           // cs:57e1
  void runBlinks();            // cs:57f0
  void flushColours();         // cs:5918
  void attractLights();        // cs:622d
  void displayFlash();         // cs:4c7d
  void displayNormal();        // cs:4cbd
  void displayInverse();       // cs:4cd4
  void displaySteady();        // cs:4d34

  /// A routine not written yet: says so, with its place.
  void todo(u16 partyLandAddress) { call(F(partyLandAddress)); }
  void effect(u16 record);  ///< plays the four-byte effect record at Party Land's address

  bool high() { return B(at::highResolution) == 0xff; }

  std::array<u8, 768> dac_{};
  SoundDriver silent_;
  bool exited_ = false;
  u16 screenRow_ = 0;
};

}  // namespace encore
