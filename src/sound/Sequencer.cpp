#include "sound/Sequencer.h"

#include <optional>

namespace pfr {

u8 SimpleSequencer::nextPosition() {
  const u8 next = position_.load(std::memory_order_relaxed);
  position_.store(static_cast<u8>(next + 1 == wrap_ ? 0 : next + 1), std::memory_order_relaxed);
  return next;
}

u8 SimpleSequencer::jump(u8 target) {
  position_.store(target, std::memory_order_relaxed);
  return nextPosition();
}

TableSequencer::TableSequencer(u8 position, u8 positionJingleStart, u8 positionSilence, bool noMusic)
    : positionJingleStart_(positionJingleStart), positionSilence_(positionSilence) {
  State s;
  s.position = position;
  s.interrupt = true;
  s.music = position;
  s.noMusic = noMusic;
  state_.store(s.pack());
}

bool TableSequencer::playJingle(const Jingle& jingle, bool force, std::optional<u8> music) {
  return update([&](State& s) {
    if (jingle.priority < s.priority && !force) return false;
    if (s.repeat == 0) s.music = s.position;
    s.position = jingle.position;
    s.interrupt = true;
    s.repeat = jingle.repeat;
    s.priority = jingle.priority;
    if (music) s.music = *music;
    return true;
  });
}

void TableSequencer::setMusic(u8 position) {
  update([&](State& s) { s.music = position; return true; });
}

void TableSequencer::resetPriority() {
  update([](State& s) { s.priority = 0; return true; });
}

void TableSequencer::setNoMusic(bool flag) {
  update([&](State& s) { s.noMusic = flag; return true; });
}

void TableSequencer::forceEndLoop() {
  update([](State& s) {
    if (s.repeat != 0) return false;
    s.repeat = 1;
    return true;
  });
}

std::optional<u8> TableSequencer::checkInterrupt() {
  u8 pos = 0;
  const bool fired = update([&](State& s) {
    if (!s.interrupt) return false;
    s.interrupt = false;
    pos = s.position;
    return true;
  });
  return fired ? std::optional<u8>(pos) : std::nullopt;
}

u8 TableSequencer::nextPosition() {
  u8 pos = 0;
  update([&](State& s) {
    // With an interrupt pending the position is about to be overridden anyway.
    if (s.interrupt) {
      pos = s.position;
      return false;
    }
    s.position = static_cast<u8>(s.position + 1);
    pos = s.position;
    return true;
  });
  return pos;
}

u8 TableSequencer::jump(u8 target) {
  u8 result = target;
  update([&](State& s) {
    u8 t = target;
    if (s.interrupt) {
      result = t;
      return false;
    }
    if (s.repeat == 1) {
      // The jingle has run out: return to the music.
      s.priority = 0;
      s.repeat = 0;
      t = s.music;
    } else if (s.repeat > 1) {
      --s.repeat;
    }
    if (t < positionJingleStart_ && s.noMusic) t = positionSilence_;
    s.position = t;
    result = t;
    return true;
  });
  return result;
}

}  // namespace pfr
