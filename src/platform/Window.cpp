#include "platform/Window.h"

#include "core/Log.h"

namespace pfr {

Window::~Window() {
  if (context_) SDL_GL_DestroyContext(context_);
  if (window_) SDL_DestroyWindow(window_);
}

bool Window::create(const std::string& title, int width, int height) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  window_ = SDL_CreateWindow(title.c_str(), width, height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window_) {
    log::error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    return false;
  }
  context_ = SDL_GL_CreateContext(window_);
  if (!context_) {
    log::error(std::string("SDL_GL_CreateContext: ") + SDL_GetError());
    return false;
  }
  SDL_GL_MakeCurrent(window_, context_);
  SDL_GL_SetSwapInterval(1);
  return true;
}

void Window::swap() { SDL_GL_SwapWindow(window_); }

void Window::setFullscreen(bool on) {
  fullscreen_ = on;
  SDL_SetWindowFullscreen(window_, on);
}

void Window::drawableSize(int& w, int& h) const { SDL_GetWindowSizeInPixels(window_, &w, &h); }

}  // namespace pfr
