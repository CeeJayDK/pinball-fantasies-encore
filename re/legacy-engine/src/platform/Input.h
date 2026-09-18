#pragma once
#include <SDL3/SDL.h>

#include <array>

namespace pfr {

/// Logical game controls, mapped from keyboard (the original key layout) and gamepads.
enum class Control {
  LeftFlipper,
  RightFlipper,
  UpperLeftFlipper,   ///< tables with a third flipper use a separate key like the original
  Plunger,
  NudgeLeft,
  NudgeRight,
  NudgeUp,
  Start,              ///< add player / start game (F1..F8 handled separately)
  Menu,               ///< ESC
  Pause,
  Count
};

class Input {
 public:
  /// Applies an SDL event. Returns true if the event was consumed.
  bool handle(const SDL_Event& e);
  void endFrame();

  bool down(Control c) const { return down_[static_cast<int>(c)]; }
  bool pressed(Control c) const { return pressed_[static_cast<int>(c)]; }
  bool released(Control c) const { return released_[static_cast<int>(c)]; }
  /// Function key pressed this frame: 1..8, or 0.
  int functionKeyPressed() const { return functionKey_; }
  /// Last text character typed this frame (for high-score initials), or 0.
  char typed() const { return typed_; }
  bool anyKeyPressed() const { return anyKey_; }

 private:
  void set(Control c, bool state);
  std::array<bool, static_cast<int>(Control::Count)> down_{}, pressed_{}, released_{};
  int functionKey_ = 0;
  char typed_ = 0;
  bool anyKey_ = false;
};

}  // namespace pfr
