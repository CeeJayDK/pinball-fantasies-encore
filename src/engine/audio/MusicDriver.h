#pragma once
// The game's sound driver, as the table and the intro know it through int 66h: it plays the
// table's module, three channels of music and a fourth for the table's effects, and tells the
// table whenever the music comes to a jump, so that the table can say where to go instead.
//
// Written from SB16.SDR (re/fantasy/SB16_SDR.lst). The drivers do not play the module as
// ProTracker would: tempo commands only ever set the ticks to a row, a tick is always a
// fiftieth of a second, and of the effects only these do anything: 0 1 2 3 4 6 9 A B C D E9 F.
// What is not the driver's is the mixing, which is done here at the sound card's own rate.
#include <array>
#include <functional>
#include <mutex>
#include <vector>

#include "engine/table/Engine.h"

namespace encore {

class MusicDriver : public SoundDriver {
 public:
  explicit MusicDriver(int outputRate = 48000);

  /// Function 0x12: the module (a .MOD file's bytes). False if it is not one.
  bool load(ByteView mod);
  /// Function 0x13: who is asked at each jump in the music where to go; it is given the
  /// place the music names and answers with the place to go to.
  std::function<u8(u8)> onJump;

  void effect(u8 sample, u8 note, u8 volume, u8 channel) override;
  u8 jump(u16 position) override;
  void volume(u16 level) override;
  void stop() override;
  u8 start() override;
  u8 status() override { return 7; }

  /// This version's pause: a stop keeps every note where it is, and the next start goes on
  /// from there. (The driver's own stop silences the channels for good.)
  bool holdOnStop = false;
  /// Function 0x18: both sides the same.
  void setMono(bool mono) { mono_ = mono; }
  /// Function 0x09: where the music is: its place in the song and the row there.
  int position() const { return position_; }
  int row() const { return (rowAt_ % 0x300) / 12; }
  /// Function 0x16: fiftieths of a second played.
  u32 ticks() const { return ticks_; }

  /// The game's time moves on: the music is played for that long, tick by tick, and what it
  /// sounds like is kept until it is asked for. Jumps are asked about from here, so the
  /// music does to the game the same thing at the same moment in every playing of it.
  void advance(double seconds);
  /// The sound made so far, for the sound card: interleaved left and right. What is not
  /// there yet is silence.
  void render(float* out, int frames);
  /// The same without anybody listening: what was made is thrown away.
  void discard();

 private:
  struct Sample {
    const i8* data = nullptr;
    u16 length = 1, loopStart = 0, loopEnd = 1;
    u8 volume = 0;
    u16 table = 0;  ///< where its finetune's periods begin in the table
  };
  struct Channel {
    const i8* data = nullptr;
    u16 position = 0, fraction = 0;  ///< where in the sample, and the 65536ths past it
    u16 end = 0, loopStart = 0, loops = 0;
    u16 volume = 0;                  ///< in 256ths of a step of the sixty-four
    u16 note = 0;                    ///< where in the table of periods its note is
    i16 period = 0;
    u32 step = 0, arpeggio1 = 0, arpeggio2 = 0;  ///< sample bytes a second, in 65536ths
    i16 slide = 0, portamento = 0, target = 0;
    u16 offset = 0, table = 0;
    u8 vibratoAt = 0, vibratoSpeed = 0, vibratoDepth = 0, retrigger = 0, retriggerLeft = 0;
    bool vibrated = false;
    enum class Tick { None, Arpeggio, Portamento, Vibrato, VibratoSlide, Slide, Retrigger } tick = Tick::None;
  };

  void tick();
  void playRow();
  void trigger(Channel& c, u8 sample, u8 note, u8 effect, u8 param);
  void everyTick(Channel& c);
  void vibrato(Channel& c);
  void slideVolume(Channel& c);
  u32 stepFor(int period) const;
  int periodAt(u16 index) const;
  void mixTick();

  int rate_;
  std::vector<u8> cells_;                 ///< the patterns, three bytes a note as the driver keeps them
  std::vector<std::vector<i8>> sampleData_;
  std::array<Sample, 31> samples_{};
  std::array<u8, 128> order_{};
  u16 length_ = 0, restart_ = 0;
  u16 position_ = 0;
  std::size_t rowAt_ = 0;                 ///< the next row, as bytes into the patterns
  u8 pendingRow_ = 0;                     ///< 0, or one more than the row a jump goes to
  u8 speed_ = 6, ticksLeft_ = 1, rowSample_ = 0;
  u32 ticks_ = 0;
  u16 master_ = 0xff;
  bool held_ = false;                     ///< stopped by this version's pause
  bool playing_ = false, loaded_ = false, mono_ = false;
  std::array<Channel, 4> ch_{};
  double due_ = 0;                        ///< ticks owed
  double tickSamples_ = 0;                ///< output frames owed to the tick being mixed
  std::mutex mutex_;
  std::vector<float> made_;               ///< sound waiting for the card
};

}  // namespace encore
