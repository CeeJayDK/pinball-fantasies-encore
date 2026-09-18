#pragma once
// The game's four-channel module player (translated from pfr's sound/player.rs). The main
// thread talks to it only through atomics: sound effects, master volume, pause, and the
// sequencer that chooses song positions.
#include <atomic>
#include <memory>

#include "sound/Mod.h"
#include "sound/Sequencer.h"

namespace pfr {

class Player {
 public:
  Player(Mod module, std::shared_ptr<Sequencer> sequencer, int sampleRate = 48000);

  /// Audio thread: fills `frames` stereo frames.
  void render(float* out, int frames);

  // Main thread.
  void playSfx(const Sfx& sfx, u8 volume);
  void setMasterVolume(u32 v) { volume_.store(v, std::memory_order_relaxed); }
  u32 masterVolume() const { return volume_.load(std::memory_order_relaxed); }
  void pause() { paused_.store(true, std::memory_order_relaxed); }
  void unpause() { paused_.store(false, std::memory_order_relaxed); }
  bool paused() const { return paused_.load(std::memory_order_relaxed); }
  /// Music ticks (50 per second) played so far.
  u32 ticks() const { return ticks_.load(std::memory_order_acquire); }

 private:
  enum class ToneFx : u8 { None, Portamento, Vibrato, Arpeggio, Retrig };
  struct Channel {
    u8 volume = 0x40;
    std::size_t sample = 0;
    u64 samplePos = 0, bytesPerFrame = 0, samplePosReload = 0;
    u8 xperiod = 0;
    u16 period = 0;
    ToneFx tone = ToneFx::None;
    u16 arpeggio[2]{};
    u16 portaTarget = 0;
    u8 portaSpeed = 0, vibPhase = 0, vibRate = 0, vibDepth = 0;
    bool volumeSlide = false;
    int slideSpeed = 0;
    u8 retrigPeriod = 0, retrigLeft = 0;
  };

  void processInterrupt();
  void playRow();
  void playNote(std::size_t ch, const SongNote& note);
  void playEffects();
  i32 playChannel(std::size_t ch);
  void setStep(Channel& c, u16 period) const;

  Mod module_;
  std::shared_ptr<Sequencer> sequencer_;
  u32 sampleRate_;
  u8 speed_ = 6, ticksLeft_ = 0;
  u32 samplesLeft_ = 0, samplesInTick_;
  std::size_t position_ = 0, row_ = 0;
  std::array<Channel, 4> channels_{};
  std::optional<u8> patternBreak_, jump_;

  std::atomic<u32> ticks_{0}, volume_{0x100}, sfx_{0};
  std::atomic<bool> paused_{false};
};

}  // namespace pfr
