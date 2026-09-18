#pragma once
#include <SDL3/SDL.h>

#include <string>

namespace pfr {

/// SDL3 window with an OpenGL 4.1 core context.
class Window {
 public:
  Window() = default;
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool create(const std::string& title, int width, int height);
  void swap();
  void setFullscreen(bool on);
  bool fullscreen() const { return fullscreen_; }
  void drawableSize(int& w, int& h) const;
  SDL_Window* handle() const { return window_; }

 private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext context_ = nullptr;
  bool fullscreen_ = false;
};

}  // namespace pfr
