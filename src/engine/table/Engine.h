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

  /// The display's memory as the original lays it out in the video card: two planes (the
  /// card's first and third), 168 bytes a row, each byte a dot, 0xf2 lit and 0x60 not.
  const std::array<std::array<u8, 0x4000>, 2>& displayMemory() const { return vram_; }
  std::array<std::array<u8, 0x4000>, 2>& displayMemory() { return vram_; }

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
  bool probeBall();         // cs:8829
  void bounce();            // cs:8e95
  void nudgeAndFlippers();  // cs:9133
  void integrate();         // cs:908f
  void stampFlippers();     // cs:9106
  void afterSteps();        // cs:59aa
  void bumperEvent();       // cs:5ace
  void pickGravity();       // cs:59d9
  void changeLayer();       // cs:5d24
  void runRules();          // cs:5989
  void rollTriggers();      // cs:5d70
  void hitTriggers();       // cs:5cb9
  void bindPlunger();
  /// One dot of a collision mask (a segment of the program's, 40 bytes a row).
  bool maskBit(u16 partyLandSegment, int x, int y);

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

  // --- the display (EngineDisplay.cpp): scripts of steps, each a routine and its arguments
  void bindDisplay();
  void displayStep();               // cs:444c
  void startScript(u16 native);     // cs:44b0
  void runStep(u16 native);         // cs:44c3
  void nextStep(u16 size);          // cs:5344 and its like: the step after this one
  void drawChar(u8 c, u16& at);     // cs:6ccd
  void drawText(u16 nativeText, u16 at);    // cs:6ca5
  void drawNumber(u16 nativeDigits, u16 at);  // cs:6c0f
  void drawCommas(u16 nativeDigits, u16 base);  // cs:6d6f
  void forgetNumber();              // cs:6bbc
  void drawScore(u16 nativeDigits, u16 at);   // the tables' second code segment
  void fillDisplay(u16 at, u16 width, u16 rows);  // cs:49eb
  void setFont(int which);          // 0 to 3: 13, 11, 8 and 5 dots high
  /// Runs one of the original's pictures that are code: a row of "store this register there".
  void compiledPicture(const u8* code, std::size_t size, u16 start, u16 base, int plane);
  u8& dot(int plane, u16 offset) { return vram_[plane][offset & 0x3fff]; }

  // --- a game's comings and goings (EngineGame.cpp)
  void bindGame();
  void newGame();              // cs:3881
  void beginBall();            // cs:37ea
  void toAttract();            // cs:5fff
  void lightsOut();            // cs:5867
  /// cs:5c3f: asks for a piece of the music (place, repeats, priority); false if something
  /// more important is playing.
  bool music(u16 nativeRecord);
  void addScore(u16 nativeTo, u16 nativeAmount);  // cs:6a5e: twelve digits, one to a byte
  void placeBall(u16 x, u16 y);
  /// Copies a shape into a collision mask (cs:5f5a): `width` bytes a row for `rows` rows.
  void patchMask(u16 partyLandSegment, u16 at, u16 nativeShape, u16 width, u16 rows);
  /// Called when a collision mask changes, for whoever keeps a copy.
  virtual void maskChanged(u16 nativeSegment, u16 offset, u8 value) { (void)nativeSegment, (void)offset, (void)value; }

  /// A routine not written yet: says so, with its place.
  void todo(u16 partyLandAddress) { call(F(partyLandAddress)); }
  void effect(u16 record);  ///< plays the four-byte effect record at Party Land's address

  bool high() { return B(at::highResolution) == 0xff; }

  std::array<u8, 768> dac_{};
  std::array<std::array<u8, 0x4000>, 2> vram_{};
  SoundDriver silent_;
  bool exited_ = false;
  u16 screenRow_ = 0;
};

}  // namespace encore
