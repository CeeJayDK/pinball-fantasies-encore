#pragma once
// The keys the game reacts to, independent of the windowing library.
#include "core/Types.h"

namespace encore {

enum class Key : u8 {
  None,
  ShiftLeft, ShiftRight, ControlLeft, ControlRight, AltLeft, AltRight,
  Space, ArrowDown, ArrowUp, ArrowLeft, ArrowRight, Enter, Escape,
  F1, F2, F3, F4, F5, F6, F7, F8,
  Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8,
  A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
};

/// 'A'..'Z' for letter keys, ' ' for space, 0 otherwise.
inline u8 keyChar(Key k) {
  if (k >= Key::A && k <= Key::Z) return static_cast<u8>('A' + (static_cast<int>(k) - static_cast<int>(Key::A)));
  return k == Key::Space ? ' ' : 0;
}

}  // namespace encore
