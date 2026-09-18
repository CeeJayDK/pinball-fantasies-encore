#pragma once
// A table's sounds.
//
// The tables do not carry sound files. Their music module holds every piece of background
// music and every jingle as a numbered position in the song, and every sound effect as a
// single note of one of the module's samples. The table's code just names positions and
// notes, which is what these records hold.
#include "core/Types.h"

namespace pfr {

/// A piece of music or a jingle: where to jump to in the module, and how it behaves.
struct Jingle {
  u8 position = 0;   ///< song position to jump to
  u8 repeat = 0;     ///< how many times it repeats before returning to the background music
  u8 priority = 0;   ///< a louder event interrupts a quieter one, never the other way round
  bool valid() const { return position != 0 || repeat != 0 || priority != 0; }
};

/// A sound effect: one note of one sample, played without disturbing the music.
struct Sfx {
  u8 sample = 0;    ///< instrument number in the module, counting from one
  u8 note = 0;      ///< note index, one being the lowest
  u8 channel = 0;   ///< counting from zero; the effects use the last channel
  bool valid() const { return sample != 0; }
};

/// The sounds every table has.
struct TableSounds {
  Sfx flipper, drained, issueBall, spring, rollInner, tickBonus, gameStart, raiseTargets;
  Jingle silence, plunger, main, attract, warnTilt, tilt, gameOverSad, gameOverHighScore,
      drainedJingle, gameStartJingle, matchStart, matchWin;
};

TableSounds extractSounds(ByteView dataSegment, int tableIndex);

/// Reads one effect record from anywhere in the data segment, for the per-element sounds
/// that bumpers and targets point at.
Sfx readSfx(ByteView dataSegment, std::size_t offset);
Sfx readSfxNear(ByteView dataSegment, std::size_t offset);

}  // namespace pfr
