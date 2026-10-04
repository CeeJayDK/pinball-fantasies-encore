#include "engine/platform/Input.h"

namespace encore {

void Input::set(Control c, bool state) {
  const int i = static_cast<int>(c);
  if (state && !down_[i]) pressed_[i] = true;
  if (!state && down_[i]) released_[i] = true;
  down_[i] = state;
}

bool Input::handle(const SDL_Event& e) {
  if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP) return false;
  const bool state = e.type == SDL_EVENT_KEY_DOWN;
  if (state && !e.key.repeat) anyKey_ = true;
  // Original layout: Left Shift / Right Shift flippers, Enter or Down = plunger,
  // Space = nudge, ESC = menu, F1-F8 add players, P pause.
  switch (e.key.scancode) {
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_Z: set(Control::LeftFlipper, state); break;
    case SDL_SCANCODE_RSHIFT: case SDL_SCANCODE_RCTRL: case SDL_SCANCODE_SLASH: set(Control::RightFlipper, state); break;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_A: set(Control::UpperLeftFlipper, state); break;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_DOWN: set(Control::Plunger, state); break;
    case SDL_SCANCODE_SPACE: case SDL_SCANCODE_UP: set(Control::NudgeUp, state); break;
    case SDL_SCANCODE_LEFT: set(Control::NudgeLeft, state); break;
    case SDL_SCANCODE_RIGHT: set(Control::NudgeRight, state); break;
    case SDL_SCANCODE_ESCAPE: set(Control::Menu, state); break;
    case SDL_SCANCODE_P: set(Control::Pause, state); break;
    default: break;
  }
  if (state && !e.key.repeat) {
    if (e.key.scancode >= SDL_SCANCODE_F1 && e.key.scancode <= SDL_SCANCODE_F8)
      functionKey_ = static_cast<int>(e.key.scancode - SDL_SCANCODE_F1) + 1;
    if (e.key.scancode >= SDL_SCANCODE_A && e.key.scancode <= SDL_SCANCODE_Z)
      typed_ = static_cast<char>('A' + (e.key.scancode - SDL_SCANCODE_A));
    else if (e.key.scancode >= SDL_SCANCODE_1 && e.key.scancode <= SDL_SCANCODE_9)
      typed_ = static_cast<char>('1' + (e.key.scancode - SDL_SCANCODE_1));
    else if (e.key.scancode == SDL_SCANCODE_0) typed_ = '0';
    else if (e.key.scancode == SDL_SCANCODE_SPACE) typed_ = ' ';
    else if (e.key.scancode == SDL_SCANCODE_BACKSPACE) typed_ = '\b';
    else if (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_KP_ENTER) typed_ = '\n';
  }
  return true;
}

void Input::endFrame() {
  pressed_.fill(false);
  released_.fill(false);
  functionKey_ = 0;
  typed_ = 0;
  anyKey_ = false;
}

}  // namespace encore
