#pragma once
// ProTracker replayer with sound-effect injection, as the original game used its
// music player to play the table's sound effects (the effects are samples in the
// table's .MOD). Rendering is deterministic and independent of the host audio API.
#include <array>
#include <mutex>

#include "data/ModFile.h"

namespace pfr {

class ModPlayer {
 public:
  static constexpr int kChannels = 4;

  explicit ModPlayer(int outputRate = 48000);

  void setModule(const ModFile* mod);  ///< The module must outlive the player.
  void play();
  void stop();
  bool playing() const { return playing_; }
  /// Restarts playback at an order-list position (0-based).
  void setPosition(int order);
  int position() const { return order_; }
  int row() const { return row_; }
  void setMasterVolume(float volume) { master_ = volume; }
  void setMusicEnabled(bool on) { musicEnabled_ = on; }
  void setInterpolation(bool on) { interpolate_ = on; }

  /// Plays a module sample (1-based index) on a channel, overriding the music on that
  /// channel until the next music note event or until the sample ends.
  /// `period` is an Amiga period (e.g. 428 = C-2); `volume` 0..64.
  void triggerSample(int channel, int sampleIndex, int period, int volume);
  /// Plays a sample by note index, counting from one, as the original's driver did.
  void triggerNote(int channel, int sampleIndex, int noteIndex, int volume);
  void stopSample(int channel);

  /// Renders interleaved stereo float frames.
  void render(float* out, int frames);

 private:
  struct Channel {
    const ModSample* sample = nullptr;
    double pos = 0;
    double step = 0;
    int period = 0;
    int targetPeriod = 0;
    int volume = 0;
    int finetune = 0;
    int portaSpeed = 0;
    int vibratoPos = 0, vibratoDepth = 0, vibratoSpeed = 0, vibratoWave = 0;
    int tremoloPos = 0, tremoloDepth = 0, tremoloSpeed = 0, tremoloWave = 0;
    int arpeggioBase = 0;
    int loopRow = 0, loopCount = 0;
    int sampleOffsetMemory = 0;
    int retrigCounter = 0;
    bool active = false;
    bool sfx = false;  ///< channel currently owned by a sound effect
    float pan = 0.5f;  ///< 0 = left, 1 = right
    u8 effect = 0, param = 0;
    int noteDelay = 0;
    const ModNote* pendingNote = nullptr;
  };

  void tick();
  void processRow();
  void processEffectsTick();
  void triggerNote(Channel& c, const ModNote& note, int chIndex);
  void setStep(Channel& c, int period);
  void mix(float* out, int frames);

  const ModFile* mod_ = nullptr;
  int rate_;
  bool playing_ = false;
  bool musicEnabled_ = true;
  bool interpolate_ = true;
  float master_ = 1.0f;
  int speed_ = 6;
  int bpm_ = 125;
  int tickCounter_ = 0;
  int order_ = 0;
  int row_ = 0;
  int patternDelay_ = 0;
  int breakRow_ = -1;
  int jumpOrder_ = -1;
  double samplesPerTick_ = 0;
  double tickAccumulator_ = 0;
  std::array<Channel, kChannels> ch_;
  std::mutex mutex_;
};

}  // namespace pfr
