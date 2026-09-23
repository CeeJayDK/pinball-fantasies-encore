#pragma once
// Decides which song position the module player moves to next. The table's sequencer
// overlays jingles (short fanfares with a priority and a repeat count) on the background
// music, the way the game does (translated from pfr's sound/controller.rs).
//
// The player runs on the audio thread and the game on the main thread, so the whole
// state lives in one atomic word updated by compare-and-swap, as in pfr.
#include <atomic>
#include <optional>

#include "assets/TableAssets.h"

namespace pfr {

class Sequencer {
 public:
  virtual ~Sequencer() = default;
  /// A new position to jump to immediately, if the game asked for one.
  virtual std::optional<u8> checkInterrupt() = 0;
  /// The position to play after the current one ends.
  virtual u8 nextPosition() = 0;
  /// A position-jump effect in the song; returns where to actually go.
  virtual u8 jump(u8 target) = 0;
};

/// Plays a module from start to end and loops (menus and intro).
class SimpleSequencer final : public Sequencer {
 public:
  explicit SimpleSequencer(u8 wrap) : wrap_(wrap) {}
  std::optional<u8> checkInterrupt() override { return std::nullopt; }
  u8 nextPosition() override;
  u8 jump(u8 target) override;

 private:
  std::atomic<u8> position_{0};
  u8 wrap_;
};

class TableSequencer final : public Sequencer {
 public:
  TableSequencer(u8 position, u8 positionJingleStart, u8 positionSilence, bool noMusic);

  /// Starts a jingle unless a higher-priority one is playing (or `force`). `music` replaces
  /// the position to return to afterwards.
  bool playJingle(const Jingle& jingle, bool force, std::optional<u8> music);
  void setMusic(u8 position);
  void resetPriority();
  void setNoMusic(bool flag);
  void forceEndLoop();
  u8 music() const { return State(state_.load(std::memory_order_acquire)).music; }
  u8 priority() const { return State(state_.load(std::memory_order_acquire)).priority; }
  bool jinglePlaying() const { return State(state_.load(std::memory_order_acquire)).repeat != 0; }

  std::optional<u8> checkInterrupt() override;
  u8 nextPosition() override;
  u8 jump(u8 target) override;

 private:
  struct State {
    u8 position = 0;
    bool interrupt = false;
    u8 repeat = 0, priority = 0, music = 0;
    bool noMusic = false;
    State() = default;
    explicit State(u32 v)
        : position(static_cast<u8>(v & 0x7f)), interrupt((v & 0x80) != 0), repeat(static_cast<u8>(v >> 8)),
          priority(static_cast<u8>(v >> 16)), music(static_cast<u8>(v >> 24 & 0x7f)), noMusic((v & 0x80000000u) != 0) {}
    u32 pack() const {
      return u32{position} | u32{interrupt} << 7 | u32{repeat} << 8 | u32{priority} << 16 | u32{music} << 24 |
             u32{noMusic} << 31;
    }
  };
  /// Applies `f` to the state atomically; `f` returns false to leave it unchanged.
  template <typename F>
  bool update(F f) {
    u32 val = state_.load(std::memory_order_acquire);
    for (;;) {
      State s(val);
      if (!f(s)) return false;
      if (state_.compare_exchange_weak(val, s.pack(), std::memory_order_release, std::memory_order_acquire)) return true;
    }
  }

  std::atomic<u32> state_;
  u8 positionJingleStart_, positionSilence_;
};

}  // namespace pfr
