// Lights. A light is a few colours of the table's picture: lit, they are as the table gives
// them; out, half as bright. Changes are queued and sent to the video card once a frame.
#include "engine/table/Engine.h"

namespace encore {

/// Adds a record (first colour, count of bytes, the bytes) to the queue, as it is or halved.
void Engine::queueColours(u16 record, bool half) {
  u16 to = W(0x3314);
  nativeB(to++) = nativeB(record++);
  const u8 count = nativeB(record++);
  nativeB(to++) = count;
  for (u8 i = 0; i < count; ++i) {
    const u8 v = nativeB(record++);
    nativeB(to++) = half ? v >> 1 : v;
  }
  W(0x3314) = to;
}

void Engine::lightOn(u8 light) { queueColours(W(0x12bd, static_cast<u16>((light - 1) * 2)), false); }
void Engine::lightOff(u8 light) { queueColours(W(0x12bd, static_cast<u16>((light - 1) * 2)), true); }

void Engine::setLight(u8 light) {
  B(0x3591, light) = 0xff;
  lightOn(light);
}

void Engine::clearLight(u8 light) {
  B(0x3591, light) = 0;
  lightOff(light);
}

void Engine::blink(u8 light, u8 start, u8 halfPeriod) {
  for (u16 slot = A(0x3555); slot != A(0x3591); slot = static_cast<u16>(slot + 4)) {
    if (nativeB(slot) != 0) continue;
    nativeB(slot) = light;
    nativeB(static_cast<u16>(slot + 1)) = start;
    nativeB(static_cast<u16>(slot + 2)) = halfPeriod;
    nativeB(static_cast<u16>(slot + 3)) = static_cast<u8>(halfPeriod << 1);
    return;
  }
}

void Engine::stopBlink(u8 light) {
  for (u16 slot = A(0x3555); slot != A(0x3591); slot = static_cast<u16>(slot + 4)) {
    if (nativeB(slot) != light) continue;
    nativeB(slot) = 0;
    return;
  }
}

void Engine::stopBlinks() {
  for (u16 i = 0, bytes = kw(0x57e1, 1); i < bytes; ++i) B(0x3555, i) = 0;
}

void Engine::runBlinks() {
  for (u16 slot = A(0x3555); slot != A(0x3591); slot = static_cast<u16>(slot + 4)) {
    const u8 light = nativeB(slot);
    if (light == 0) continue;
    u8& count = nativeB(static_cast<u16>(slot + 1));
    if (count == 0) {
      lightOn(light);
    } else if (count == nativeB(static_cast<u16>(slot + 2))) {
      lightOff(light);
    } else if (count == nativeB(static_cast<u16>(slot + 3))) {
      count = 0;
      lightOn(light);
    }
    ++count;
  }
}

void Engine::flushColours() {
  if (B(0x354a) == 0xff) return;
  B(0x354a) = 0xff;
  if (CB(0x4f23) != 0xff) {
    for (u16 at = A(0x2f2c); at != W(0x3314);) {
      B(0x354d) = 0;
      std::size_t colour = std::size_t{nativeB(at++)} * 3;
      const u8 count = nativeB(at++);
      for (u8 i = 0; i < count; ++i, ++colour) dac_[colour % 768] = nativeB(at++) & 0x3f;
    }
  }
  W(0x3314) = A(0x2f2c);
  B(0x354a) = 0;
}

/// cs:622d: the lights' show while no game is played. Each entry counts up; at one count its
/// light goes out, at another it comes on and the count starts again.
void Engine::attractLights() {
  for (u16 e = A(0x0f41); nativeW(e) != 0xffff; e = static_cast<u16>(e + 10)) {
    const u16 count = static_cast<u16>(nativeW(e) + 1);
    nativeW(e) = count;
    if (count == nativeW(static_cast<u16>(e + 4))) {
      lightOff(nativeB(static_cast<u16>(e + 8)));
    } else if (count == nativeW(static_cast<u16>(e + 6))) {
      lightOn(nativeB(static_cast<u16>(e + 8)));
      nativeW(e) = nativeW(static_cast<u16>(e + 2));
    }
  }
}

void Engine::displayFlash() {
  if (B(0x3396) != 0xff) return;
  if (--W(0x3392) != 0) return;
  W(0x3392) = W(0x3390);
  W(0x3394) = W(0x3394) ^ 0xff;
  queueColours(W(0x3394) == 0 ? A(0x132d) : A(0x12b7), false);
}

void Engine::displayNormal() { queueColours(A(0x12b7), false); }
void Engine::displayInverse() { queueColours(A(0x132d), false); }

void Engine::displaySteady() {
  B(0x3396) = 0;
  queueColours(A(0x12b7), false);
}

}  // namespace encore
