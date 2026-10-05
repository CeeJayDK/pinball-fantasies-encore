#pragma once
// The engine the four tables share, written again routine by routine from Party Land's
// program (docs/own-engine.md). A comment "cs:1234" is where the routine is in TABLE1.PRG.
#include <array>
#include <string_view>

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
  /// Function 0x06: the volume, 0x100 for all of it.
  virtual void volume(u16 level) { (void)level; }
  /// Function 0x0f: stop the music.
  virtual void stop() {}
  /// Function 0x04: start it.
  virtual u8 start() { return 0; }
  /// Function 0x15: what is playing; bit 3 is set while an effect still sounds. The silent
  /// driver always answers that one does.
  virtual u8 status() { return 0x0a; }
};

class Engine : public Program {
 public:
  Engine(ByteView prg, int table);

  /// The options, as PINBALL.CFG has them.
  struct Options {
    bool fiveBalls = false;
    bool lowAngle = false;
    u8 scrolling = 1;      ///< 0 hard, 1 medium, 2 soft
    bool musicOff = false;
    bool highResolution = true;
    bool mono = false;
  };
  /// The program's start-up (cs:2f9b), up to where its own loop begins: once, before the
  /// first frame. `bestScores` is the table's TABLEn.HI, 64 bytes, if there is one.
  void start(const Options& options, ByteView bestScores = {});

  /// A key goes down or up, as the keyboard says it: a scancode, bit 7 set for up
  /// (cs:3e44, the table's keyboard interrupt).
  void key(u8 scancode);
  /// One video frame, as the driver's two callbacks run it (cs:41c6, cs:55db), then the
  /// program's own loop (cs:35fd) `loopsPerFrame` times.
  void frame();
  int loopsPerFrame = 8;
  /// As the silent driver: the music's callback is called every time the driver is polled.
  bool pollCallsMusic = true;
  /// The sound driver's question at each jump in the music (cs:3a6a): the place the music
  /// names, and the place to go to instead. For a driver that plays; the silent one is
  /// answered through `pollCallsMusic`.
  u8 musicAsks(u8 place) { return musicCallback(place); }
  /// The original lets the number of players be put below the player whose turn it is while
  /// more may still join, and then goes astray (docs/own-engine.md). With this, such a key
  /// is taken for no key.
  bool refuseFewerPlayers = false;
  /// The program asked to end.
  bool exited() const { return exited_; }
  /// Where the moving things were when the original last drew them (which is not always
  /// where they are now: some are drawn before a frame's steps and some after).
  struct Shown {
    i16 ballX = 0, ballY = 0;
    bool ramps = false;              ///< the ball is on the ramps: other things hide it there
    std::array<u16, 3> flipper{};    ///< which of its pictures each flipper shows
  };
  const Shown& shown() const { return shown_; }
  /// A flipper is drawn standing another way than it was last drawn: its record, and which
  /// of its pictures it showed and shows now. The original gets from one to the other by
  /// lists of changes, and what is left on the screen depends on the way taken.
  std::function<void(u16 record, u16 was, u16 now)> onFlipperDrawn;
  /// The row of the picture at the top of the screen.
  u16 screenRow() const { return screenRow_; }

  // --- what this version lets be changed while a table is played. The original takes its
  // options once, as it starts; these put right what its start-up made of them.
  /// How steeply the table lies: 0 low, 1 high, and 2 steeper than the original has it. At
  /// that one the pull down the table is as much greater again as high is over low, and so
  /// that the top of the table can still be reached the flippers' push, the plunger and the
  /// fastest the ball may go are a quarter more.
  void setAngle(int angle);
  int angle() const { return angle_; }
  /// How quickly the screen follows the ball: 0 hard, 1 medium, 2 soft.
  void setScrolling(u8 scrolling);
  void toggleMusic() { musicKey(); }
  bool musicIsOff() { return B(0x231a) != 0; }
  /// The game is paused (the original then waits for a key and does nothing else).
  bool isPaused() const { return pause_ != Pause::No; }
  bool asksToQuit() const { return pause_ == Pause::YesOrNo; }
  /// A line on the display, in the letters the pause is told in: capitals, digits and
  /// spaces, and \ and ] for brackets. What the display showed is not kept: for while the
  /// game is paused, which keeps it and puts it back.
  void write(std::string_view text);
  /// Where the ball was after each of its last steps (there are four to a frame), the newest
  /// last: in 1024ths of a dot.
  struct Step {
    i32 x = 0, y = 0;
  };
  static constexpr std::size_t kSteps = 14;
  const std::array<Step, kSteps>& steps() const { return steps_; }
  /// How many of them are of the ball now in play (the last ones).
  std::size_t stepsKept() const { return stepsKept_; }

  /// The video card's memory, four planes of 64 KB, as far as the table's logic depends on
  /// it. The display is at its start: in the first and third planes, 168 bytes a row, each
  /// byte a dot, 0xf2 lit and 0x60 not. Past the picture are the copies of three masks that the
  /// original reads the ramps' slopes and materials from, and, over parts of those, the
  /// pictures it keeps there to draw with: what it reads there, the engine must read too.
  const std::array<std::vector<u8>, 4>& videoMemory() const { return video_; }
  std::array<std::vector<u8>, 4>& videoMemory() { return video_; }

  /// The video card's colours as the table has set them: 256 of red, green, blue, 0-63.
  const std::array<u8, 768>& colours() const { return dac_; }
  std::array<u8, 768>& colours() { return dac_; }

  SoundDriver* sound = &silent_;

 protected:
  // --- the frame (EngineFrame.cpp)
  void frameCallback();
  void midFrameCallback();
  void mainLoop();
  void drawBall();          // cs:4140
  void drawFlipper(int which);  // cs:550e
  void attractFrame();      // cs:6011
  void attractMidFrame();   // cs:60c5
  void scroll();            // cs:4018
  u8 musicCallback(u8 al);  // cs:3a6a
  void keyExtended(u8 al);  // cs:3f8a
  void typeCheat();         // cs:3549
  void playersKey();        // cs:33f9
  void pauseKey();          // cs:31cd
  void paused();            // the same routine's waits for a key, a frame at a time
  void saveDisplay();       // cs:4a6a
  void restoreDisplay();    // cs:4b2a
  void message(u16 nativeText, u16 at);  // cs:4aae: the display cleared, and a line on it
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
  /// A byte of one of the three mask copies in video memory, by its place in the mask
  /// (cs:5a48, cs:5a80): 0 the ramps' marks, 1 the ramps' slopes, 2 the playfield's slopes.
  u8 copiedMask(int which, u16 offset);

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
  void compiledPicture(const u8* code, std::size_t size, u16 start, u16 base, int plane, u8 litDot, u8 unlitDot);
  /// The two values a dot of the display has in video memory, which differ from table to table.
  u8 lit() const { return kb(0x4afa, 1); }
  u8 unlit() const { return kb(0x4b01, 1); }
  u8& dot(int plane, u16 offset) { return video_[static_cast<std::size_t>(plane * 2)][offset]; }

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
  /// cs:5b9f: an award: its music if nothing more important plays, its score and bonus, and
  /// its script on the display. True if the script was started.
  bool award(u16 nativeRecord);
  /// cs:5b40: the same, whatever else is playing or showing.
  bool awardAlways(u16 nativeRecord);
  /// cs:5c98: lights the next of a row of lights, one more each time; true when that was
  /// the last, and the row starts again.
  bool nextOfRow(u16 nativeRow);
  /// The timer whose turn it is gives up its slot without being counted off (as cs:140b).
  void dropTimer() { nativeW(W(0x3381)) = F(0x69fc); }
  /// Copies a shape into a collision mask (cs:5f5a): `width` bytes a row for `rows` rows.
  void patchMask(u16 partyLandSegment, u16 at, u16 nativeShape, u16 width, u16 rows);
  /// The same by the table's own segment; `step` is how far apart the shape's rows are.
  void copyShape(u16 nativeSegment, u16 at, u16 nativeShape, u16 width, u16 rows, u16 step);
  /// Called when a collision mask changes, for whoever keeps a copy.
  virtual void maskChanged(u16 nativeSegment, u16 offset, u8 value) { (void)nativeSegment, (void)offset, (void)value; }

  /// A routine not written yet: says so, with its place.
  void todo(u16 partyLandAddress) { call(F(partyLandAddress)); }
  void effect(u16 record);  ///< plays the four-byte effect record at Party Land's address

  bool high() { return B(at::highResolution) == 0xff; }

  std::array<u8, 768> dac_{};
  std::array<std::vector<u8>, 4> video_;
  SoundDriver silent_;
  bool exited_ = false;
  enum class Pause { No, AnyKey, YesOrNo } pause_ = Pause::No;
  u16 screenRow_ = 0;
  Shown shown_;
  int angle_ = 1;
  std::array<i16, 2> speedLimits_{};  ///< as the start-up left them, while the steeper angle has them greater
  std::array<Step, kSteps> steps_{};
  std::size_t stepsKept_ = 0;
};

}  // namespace encore
